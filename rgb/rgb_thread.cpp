#include "board.h"              // for THREAD_PRIO_RGB
#include "is31fl3733.h"
#include "msg.h"                // for msg_init_queue(), msg_try_receive(), msg_send()
#include "thread_flags.h"       // for thread_flags_wait_any(), thread_flags_set(), ...
#include "ztimer.h"             // for ztimer_set() and ztimer_now()

#define ENABLE_DEBUG    (0)
#include "debug.h"

#include "adc_input.hpp"        // for v_5v and V_5V_MID
#include "color.hpp"            // for CIE1931_CURVE[]
#include "effects.hpp"
#include "features.hpp"         // for RGB_DISABLE_WHEN_USB_SUSPENDS
#include "led_conf.hpp"         // for IS31_LEDS[]
#include "rgb_thread.hpp"



rgb_thread_tl<true>::rgb_thread_tl()
{
    is31_init();

    m_pid = thread_create(
        m_stack, sizeof(m_stack),
        THREAD_PRIO_RGB,
        THREAD_CREATE_STACKTEST,
        _rgb_thread, this, "rgb_thread");

    m_pthread = thread_get_unchecked(m_pid);
}

void* rgb_thread_tl<true>::_rgb_thread(void* arg)
{
    rgb_thread_tl* const that = static_cast<rgb_thread_tl*>(arg);

    // The msg_init_queue() should be called from the queue-owning thread.
    msg_init_queue(that->m_queue, KEY_EVENT_QUEUE_SIZE);

    is31_switch_all_leds_on_off(true);

    if constexpr ( !RGB_DISABLE_WHEN_USB_SUSPENDS )
        that->m_gcr.enable();

    msg_t msg;
    while ( true ) {
        // Zzz
        thread_flags_t flags = thread_flags_wait_any(
            FLAG_KEY_EVENT
            | FLAG_USB_SUSPEND
            | FLAG_USB_RESUME
            | FLAG_ADJUST_GCR
            | FLAG_CHANGE_LED_STATE
            | FLAG_SET_EFFECT
            | FLAG_TIMEOUT
            );

        if constexpr ( RGB_DISABLE_WHEN_USB_SUSPENDS ) {
            if ( flags & FLAG_USB_SUSPEND )
                that->m_gcr.disable();

            if ( flags & FLAG_USB_RESUME )
                that->m_gcr.enable();
        }

        if ( flags & FLAG_ADJUST_GCR )
            that->m_gcr.adjust();

        if ( flags & FLAG_CHANGE_LED_STATE )
            that->change_led_state();

        if ( flags & FLAG_SET_EFFECT )
            that->initialize_effect();

        if ( flags & FLAG_TIMEOUT )
            that->refresh_effect();

        if ( flags & FLAG_KEY_EVENT )
            while ( msg_try_receive(&msg) == 1 )
                that->process_key_event(msg.content.value, msg.type);
    }

    return nullptr;
}

void rgb_thread_tl<true>::signal_report_5v()
{
    if ( m_gcr.is_enabled() )
        if ( (adc_input::v_5v.level() > V_5V_MID && m_gcr.raise())
          || (adc_input::v_5v.level() < V_5V_MID && m_gcr.lower()) )
            thread_flags_set(m_pthread, FLAG_ADJUST_GCR);
}

void rgb_thread_tl<true>::gcr_t::disable()
{
    m_enabled = false;
    is31_set_gcr(m_current_gcr = m_desired_gcr = 0);
    is31_switch_unlock_ssd(false);
}

void rgb_thread_tl<true>::gcr_t::adjust()
{
    if ( m_current_gcr != m_desired_gcr ) {
        if ( m_current_gcr == 0 )
            is31_switch_unlock_ssd(true);
        is31_set_gcr(m_current_gcr = m_desired_gcr);
        if ( m_current_gcr == 0 )
            is31_switch_unlock_ssd(false);

        DEBUG("Rgb: v_5v=%d v_con1=%d v_con2=%d gcr=%d\n",
            adc_input::v_5v.read(),
            adc_input::v_con1.read(),
            adc_input::v_con2.read(),
            m_current_gcr
        );
    }
}

void rgb_thread_tl<true>::set_effect(effect_t& effect)
{
    m_peffect = &effect;
    thread_flags_set(m_pthread, FLAG_SET_EFFECT);
}

void rgb_thread_tl<true>::initialize_effect()
{
    for ( uint8_t led_id = 0 ; led_id < KEY_LED_COUNT ; led_id++ ) {
        const rgb_t rgb = m_peffect->initial_update(led_id);
        is31_set_color(IS31_LEDS[led_id], rgb.r, rgb.g, rgb.b);
    }

    is31_refresh_colors();
    m_peffect->initial_update_done();
}

void rgb_thread_tl<true>::signal_key_event(uint8_t led_id, bool pressed)
{
    if ( m_peffect ) {
        msg_t msg;
        msg.type = pressed;
        msg.content.value = led_id;
        // will block the caller (matrix_thread) if m_queue if full.
        msg_send(&msg, m_pid);
    }
}

void rgb_thread_tl<true>::process_key_event(uint8_t led_id, bool pressed)
{
    const ohsv_t ohsv =
        m_peffect->process_key_event(led_id, ztimer_now(ZTIMER_MSEC), pressed);

    if ( ohsv && !m_indicators.is_lit(led_id) ) {
        const rgb_t rgb = *ohsv;
        is31_set_color(IS31_LEDS[led_id], rgb.r, rgb.g, rgb.b);

        // Todo: We could do refresh_colors only if m_gcr.is_set() is true, which then
        //  means we should do it on FLAG_USB_RESUME, but instead we would better turn
        //  off the entire effect on suspend and turn it on on resume. To enable this
        //  we have to support the discontinuity of the effects (e.g. a key can be
        //  pressed in suspend and released after resume.)
        is31_refresh_colors();
    }
}

void rgb_thread_tl<true>::refresh_effect()
{
    const uint32_t now = ztimer_now(ZTIMER_MSEC);
    for ( uint8_t led_id = 0 ; led_id < KEY_LED_COUNT ; led_id++ ) {
        // If led_id corresponds to a (lit) indicator we skip refreshing the effect on it,
        // leaving it unchanged.
        if ( m_indicators.is_lit(led_id) )
            continue;

        if ( const ohsv_t ohsv = m_peffect->update(led_id, now) ) {
            const rgb_t rgb = *ohsv;
            is31_set_color(IS31_LEDS[led_id], rgb.r, rgb.g, rgb.b);
        }
    }

    is31_refresh_colors();
    m_peffect->update_done();
}

void rgb_thread_tl<true>::change_led_state()
{
    for ( uint8_t led_id : m_indicators.led_ids )
        if ( led_id != NO_LED ) {
            rgb_t rgb { 0, 0, 0 };  // black (i.e. turn it off)

            if ( const ohsv_t ohsv = m_indicators.is_lit(led_id) )
                rgb = *ohsv;

            else if ( m_peffect ) {
                if ( m_peffect->is_updating(led_id) )
                    continue;
                rgb = m_peffect->initial_update(led_id);
            }
            // else: Turn it off if no Effect is set up.

            is31_set_color(IS31_LEDS[led_id], rgb.r, rgb.g, rgb.b);
        }

    is31_refresh_colors();
}
