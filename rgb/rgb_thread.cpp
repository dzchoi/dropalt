#include "board.h"              // for THREAD_PRIO_RGB
#include "is31fl3733.h"
#include "led_conf.hpp"         // for is31_leds[]
#include "msg.h"                // for msg_init_queue(), msg_try_receive(), msg_send()
#include "ztimer.h"             // for ztimer_now(), ztimer_set()

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "color.hpp"            // for CIE1931_CURVE[]
#include "effects.hpp"
#include "features.hpp"         // for RGB_DISABLE_WHEN_USB_SUSPENDS, ...
#include "rgb_thread.hpp"



rgb_thread_tl<true>::rgb_thread_tl()
{
    is31_init();

    m_pid = thread_create(
        m_stack, sizeof(m_stack),
        THREAD_PRIO_RGB,
        THREAD_CREATE_STACKTEST,
        _rgb_thread, this, "rgb_thread");

    m_pthread = thread_get(m_pid);
}

void* rgb_thread_tl<true>::_rgb_thread(void* arg)
{
    rgb_thread_tl* const that = static_cast<rgb_thread_tl*>(arg);

    // The msg_init_queue() should be called from the queue-owning thread.
    msg_init_queue(that->m_queue, KEY_EVENT_QUEUE_SIZE);

    is31_switch_all_leds_on_off(true);

    // Release the Software Shutdown (SSD) now if RGB_DISABLE_WHEN_USB_SUSPENDS is false,
    // so LEDs start working now. Otherwise, they will start later when USB is resumed
    // (FLAG_USB_RESUME).
    if constexpr ( !RGB_DISABLE_WHEN_USB_SUSPENDS )
        that->m_gcr.set(RGB_LED_GCR_MAX);

    msg_t msg;
    while ( true ) {
        // Zzz
        thread_flags_t flags = thread_flags_wait_any(
            FLAG_KEY_EVENT
            | FLAG_USB_SUSPEND
            | FLAG_USB_RESUME
            | FLAG_CHANGE_GCR
            | FLAG_SET_EFFECT
            | FLAG_TIMEOUT
            );

        if constexpr ( RGB_DISABLE_WHEN_USB_SUSPENDS ) {
            if ( flags & FLAG_USB_SUSPEND )
                that->m_gcr.clear();

            if ( flags & FLAG_USB_RESUME )
                that->m_gcr.set(RGB_LED_GCR_MAX);
        }

        if ( flags & FLAG_CHANGE_GCR )
            that->m_gcr.change();

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

void rgb_thread_tl<true>::gcr_t::clear()
{
    is31_set_gcr(m_current_gcr = m_desired_gcr = 0);
    is31_switch_unlock_ssd(false);
}

void rgb_thread_tl<true>::gcr_t::set(uint8_t gcr)
{
    m_desired_gcr = gcr;
    thread_flags_set(obj().m_pthread, FLAG_CHANGE_GCR);
}

void rgb_thread_tl<true>::gcr_t::change()
{
    if ( m_current_gcr != m_desired_gcr ) {
        if ( m_current_gcr == 0 )
            is31_switch_unlock_ssd(true);
        is31_set_gcr(m_desired_gcr > m_current_gcr ? ++m_current_gcr : --m_current_gcr);
        if ( m_current_gcr == 0 )
            is31_switch_unlock_ssd(false);

        if ( m_current_gcr != m_desired_gcr )
            ztimer_set(ZTIMER_MSEC, &m_timer, m_period);
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
        is31_set_color(is31_leds[led_id], rgb.r, rgb.g, rgb.b);
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
        // will block sending if m_queue if full.
        msg_send(&msg, m_pid);
    }
}

void rgb_thread_tl<true>::process_key_event(uint8_t led_id, bool pressed)
{
    const auto ohsv =
        m_peffect->process_key_event(led_id, ztimer_now(ZTIMER_MSEC), pressed);

    if ( ohsv ) {
        const rgb_t rgb = *ohsv;
        is31_set_color(is31_leds[led_id], rgb.r, rgb.g, rgb.b);

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
        const auto ohsv = m_peffect->update(led_id, now);
        if ( ohsv ) {
            const rgb_t rgb = *ohsv;
            is31_set_color(is31_leds[led_id], rgb.r, rgb.g, rgb.b);
        }
    }

    is31_refresh_colors();
    m_peffect->update_done();
}
