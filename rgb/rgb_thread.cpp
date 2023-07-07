#define LOCAL_LOG_LEVEL LOG_NONE

#include "board.h"              // for THREAD_PRIO_RGB
#include "is31fl3733.h"
#include "log.h"
#include "msg.h"                // for msg_init_queue(), msg_try_receive(), msg_send()
#include "thread_flags.h"       // for thread_flags_wait_any(), thread_flags_set(), ...
#include "ztimer.h"             // for ztimer_set() and ztimer_now()

#include "adc_input.hpp"        // for v_5v and V_5V_MID
#include "color.hpp"            // for CIE1931_CURVE[]
#include "effects.hpp"
#include "features.hpp"         // for RGB_DISABLE_WHEN_USB_SUSPENDS
#include "led_conf.hpp"         // for IS31_LEDS[]
#include "pmap.hpp"             // for led_id(), lamp_id(), is_lamp_lit()
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
            FLAG_SLOT_EVENT
            | FLAG_USB_SUSPEND
            | FLAG_USB_RESUME
            | FLAG_ADJUST_GCR
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

        if ( flags & FLAG_SET_EFFECT )
            that->initialize_effect();

        if ( flags & FLAG_TIMEOUT )
            that->refresh_effect();

        if ( flags & FLAG_SLOT_EVENT )
            while ( msg_try_receive(&msg) == 1 )
                that->process_slot_event(
                    static_cast<key::pmap_t*>(msg.content.ptr), slot_event_t(msg.type));
    }

    return nullptr;
}

void rgb_thread_tl<true>::signal_report_v_5v()
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

        LOG_DEBUG("Rgb: v_5v=%d v_con1=%d v_con2=%d gcr=%d\n",
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

void rgb_thread_tl<true>::signal_slot_event(key::pmap_t* slot, slot_event_t event)
{
    if ( m_peffect ) {
        msg_t msg;
        msg.type = event;
        msg.content.ptr = slot;
        // will block the caller (matrix_thread) if m_queue if full.
        msg_send(&msg, m_pid);
    }
}

void rgb_thread_tl<true>::process_slot_event(key::pmap_t* slot, slot_event_t event)
{
    const uint8_t led_id = slot->led_id();
    assert( led_id != NO_LED );
    rgb_t rgb { 0, 0, 0 };  // black (i.e. turn it off)

    switch ( event ) {
        case KEY_RELEASED:
        case KEY_PRESSED: {
            const ohsv_t ohsv =
                m_peffect->process_key_event(led_id, ztimer_now(ZTIMER_MSEC), event);

            if ( ohsv && !slot->is_lamp_lit() )
                rgb = *ohsv;
            else
                return;

            // Todo: We could do is31_refresh_colors() only if m_gcr.is_set() is true,
            //  which then means we should do it on FLAG_USB_RESUME, but instead we would
            //  better turn off the entire effect on suspend and turn it on on resume.
            //  To enable this we have to support the discontinuity of the effects
            //  (e.g. a key can be pressed in suspend and released after resume.)
            break;
        }

        case LAMP_CHANGED: {
            if ( const ohsv_t ohsv = slot->is_lamp_lit() )
                rgb = *ohsv;

            else if ( m_peffect ) {
                if ( !m_peffect->is_updating(led_id) )
                    rgb = m_peffect->initial_update(led_id);
            }
            // else: Turn it off if no Effect is set up.

            break;
        }
    }

    is31_set_color(IS31_LEDS[led_id], rgb.r, rgb.g, rgb.b);
    is31_refresh_colors();
}

void rgb_thread_tl<true>::refresh_effect()
{
    const uint32_t now = ztimer_now(ZTIMER_MSEC);

    for ( auto slot = led_iter::begin() ; slot != led_iter::end() ; ++slot ) {
        const uint8_t led_id = slot->led_id();
        // If slot has an indicator lamp and if it is lit we skip refreshing the effect
        // on it, leaving it unchanged.
        if ( !slot->is_lamp_lit() )
            if ( const ohsv_t ohsv = m_peffect->update(led_id, now) ) {
                const rgb_t rgb = *ohsv;
                is31_set_color(IS31_LEDS[led_id], rgb.r, rgb.g, rgb.b);
            }
    }

    is31_refresh_colors();
    m_peffect->update_done();
}
