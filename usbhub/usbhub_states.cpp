#include "assert.h"
#include "board.h"              // for LED0_ON, LED0_OFF
#include "is31fl3733.h"         // for is31_init(), is31_enable_all_leds()
#include "log.h"
#include "usb2422.h"            // for usbhub_init(), usbhub_select_host_port(), ...
#include "ztimer.h"             // for ztimer_set_timeout_flag(), ztimer_remove(), ...

#include <utility>              // for std::swap()
#include "adc.hpp"              // for sync_measure(), is_host_connected(), ...
#include "config.hpp"           // for ENABLE_RGB_LED, DEBUG_LED_BLINK_PERIOD_MS, ...
#include "main_thread.hpp"      // for main_thread::signal_dte_state()
#include "persistent.hpp"       // for persistent `last_host_port`
#include "rgb_gcr.hpp"          // for rgb_gcr::enable(), rgb_gcr::disable()
#include "usbhub_states.hpp"
#include "usbhub_thread.hpp"    // for usbhub_thread::signal_event()



void usbhub_state::init()
{
    jump_to<state_determine_host>();
}

void usbhub_state::help_process_usb_suspend()
{
    if ( automatic_switchover_enabled && !v_host->sync_measure().is_host_connected() )
        // Automatic switchover upon cable break.
        transition_to<state_usb_suspend>().perform_switchover();
    else
        transition_to<state_usb_suspend>();
}

void usbhub_state::help_process_usb_resume()
{
    LOG_DEBUG("USBHUB: acquired host port %d @%lu", v_host->line, ztimer_now(ZTIMER_MSEC));

    if ( v_extra->sync_measure().is_device_connected() )
        transition_to<state_extra_enabled>();
    else
        transition_to<state_extra_disabled>();
}

void usbhub_state::help_process_usbport_switchover()
{
    // We could allow switchover only when v_extra->is_host_connected(), but it would
    // better be more permissive and let users decide (e.g. switchover to an unconnected
    // port.)
    if ( !v_extra->sync_measure().is_device_connected() ) {
        // Explicitly signal FLAG_DTE_DISABLED to main_thread, since USB disconnection
        // at switchover doesn't trigger it automatically.
        if constexpr ( ENABLE_CDC_ACM )
            main_thread::signal_dte_state(false);

        // Disable the indicator lamps now rather than maintaining the old ones and
        // resetting them upon receiving SET_REPORT from the new host.
        main_thread::signal_lamp_state(0);

        transition_to<state_usb_suspend>().perform_switchover();
    }
    else
        LOG_WARNING("USBHUB: switchover not allowed to extra device!");
}



void state_determine_host::begin()
{
    LOG_DEBUG("USBHUB: state_determine_host");
    if constexpr ( DEBUG_LED_BLINK_PERIOD_MS > 0 )
        LED0_ON;
    ztimer_set_timeout_flag(ZTIMER_MSEC, &retry_timer,
        DEBUG_LED_BLINK_PERIOD_MS > 0
        ? DEBUG_LED_BLINK_PERIOD_MS : DEFAULT_USB_RETRY_PERIOD_MS);

    if ( likely(v_extra == nullptr) ) {
        // Initialize USB2422.
        // Could be called from usbhub_thread::init(), but usbhub_init() will take a while
        // and we may better call it from thread context.
        usbhub_init();

        // Initialize LEDs.
        if constexpr ( ENABLE_RGB_LED ) {
            is31_init();
            is31_enable_all_leds(true);
        }

        // Continuously monitor v_5v throughout runtime.
        adc::v_5v.schedule_periodic();
    }

    // Handle the scenario where state_determine_host is re-entered (for future use).
    else
        v_extra->schedule_cancel();

    // Disconnect the current host.
    // Note: Disabling the upstream mux (setting SR_CTRL_E_UP_N = 1) or selecting a
    // different host (changing SR_CTRL_S_UP) disconnects the current host, breaking the
    // data (D+) line. The data line of the extra port (SR_CTRL_E_DN1_N) is also
    // disconnected, but the power line (SR_CTRL_E_VBUS_x) remains active.
    // Immediately after disconnection, the FLAG_USB_SUSPEND event is triggered, and
    // process_usb_suspend() is invoked. This event, however, is ignored while in
    // state_determine_host.
    usbhub_disable_all_ports();

    uint8_t desired_port = USB_PORT_1;  // Default value for `last_host_port`.
    persistent::get("last_host_port", desired_port);
    LOG_DEBUG("USBHUB: try port %d first @%lu", desired_port, ztimer_now(ZTIMER_MSEC));

    usbhub_select_host_port(desired_port);
    if ( desired_port == USB_PORT_1 ) {
        v_host  = &adc::v_con1;
        v_extra = &adc::v_con2;
    } else {
        v_host  = &adc::v_con2;
        v_extra = &adc::v_con1;
    }

    // Note that state_determine_host measures v_host (during power-up) while the other
    // states measure v_extra.
    v_host->schedule_periodic();
}

void state_determine_host::isr_process_v_con_report()
{
    if ( v_host->is_host_connected() ) {
        v_host->schedule_cancel();
        LOG_DEBUG("USBHUB: determined host port %d @%lu",
            v_host->line, ztimer_now(ZTIMER_MSEC));
    }
}

void state_determine_host::process_timeout()
{
    if constexpr ( DEBUG_LED_BLINK_PERIOD_MS > 0 )
        LED0_TOGGLE;
    ztimer_set_timeout_flag(ZTIMER_MSEC, &retry_timer,
        DEBUG_LED_BLINK_PERIOD_MS > 0
        ? DEBUG_LED_BLINK_PERIOD_MS : DEFAULT_USB_RETRY_PERIOD_MS);

    if ( !v_host->is_host_connected() ) {
        v_host->schedule_cancel();

        const uint8_t desired_port = v_extra->line;  // == usbhub_extra_port();
        LOG_DEBUG("USBHUB: switchover to port %d @%lu",
            desired_port, ztimer_now(ZTIMER_MSEC));

        usbhub_select_host_port(desired_port);
        std::swap(v_host, v_extra);

        v_host->schedule_periodic();
    }
}

void state_determine_host::end()
{
    ztimer_remove(ZTIMER_MSEC, &retry_timer);

    v_host->schedule_cancel();

    // Remember the host port when acquired only from state_determine_host.
    persistent::set("last_host_port", v_host->line);

    automatic_switchover_enabled = v_host->is_host_connected();
    if ( !automatic_switchover_enabled )
        LOG_WARNING("USBHUB: automatic switchover disabled");

    // From now on, v_extra will be measured periodically.
    v_extra->schedule_periodic();

    if ( main_thread::is_dfu_mode() ) {
        if constexpr ( DEBUG_LED_BLINK_PERIOD_MS > 0 )
            LED0_ON;
    }
    else {
        if constexpr ( DEBUG_LED_BLINK_PERIOD_MS > 0 )
            LED0_OFF;
        // Enable GCR at boot if RGB_DISABLE_DURING_USB_SUSPEND is false.
        if constexpr ( ENABLE_RGB_LED && !RGB_DISABLE_DURING_USB_SUSPEND )
            rgb_gcr::enable();
    }
}



void state_usb_suspend::begin()
{
    LOG_DEBUG("USBHUB: state_usb_suspend");
    if ( !main_thread::is_dfu_mode() ) {
        if constexpr ( ENABLE_RGB_LED && RGB_DISABLE_DURING_USB_SUSPEND )
            rgb_gcr::disable();
        if constexpr ( DEBUG_LED_BLINK_PERIOD_MS > 0 )
            LED0_ON;
    }

    if constexpr ( DEBUG_LED_BLINK_PERIOD_MS > 0 )
        ztimer_set_timeout_flag(ZTIMER_MSEC, &blink_timer, DEBUG_LED_BLINK_PERIOD_MS);

    // v_extra (and v_host as well) is not measured in state_usb_suspend.
    v_extra->schedule_cancel();
}

void state_usb_suspend::perform_switchover()
{
    assert( v_host != nullptr && v_extra != nullptr );

    const uint8_t desired_port = v_extra->line;  // == usbhub_extra_port();
    LOG_DEBUG("USBHUB: switchover to port %d @%lu", desired_port, ztimer_now(ZTIMER_MSEC));

    usbhub_select_host_port(desired_port);
    std::swap(v_host, v_extra);
    // We stay in this state after performing the switchover.
}

void state_usb_suspend::process_timeout()
{
    if constexpr ( DEBUG_LED_BLINK_PERIOD_MS > 0 ) {
        LED0_TOGGLE;
        ztimer_set_timeout_flag(ZTIMER_MSEC, &blink_timer, DEBUG_LED_BLINK_PERIOD_MS);
    }
}

void state_usb_suspend::end()
{
    if constexpr ( DEBUG_LED_BLINK_PERIOD_MS > 0 ) {
        if ( main_thread::is_dfu_mode() )
            LED0_ON;
        else
            LED0_OFF;
        ztimer_remove(ZTIMER_MSEC, &blink_timer);
    }

    automatic_switchover_enabled = v_host->sync_measure().is_host_connected();
    if ( !automatic_switchover_enabled )
        LOG_WARNING("USBHUB: automatic switchover disabled");

    // From now on, v_extra will be measured periodically.
    v_extra->schedule_periodic();
}



void state_extra_disabled::begin()
{
    usbhub_enable_extra_port(v_extra->line, false);
    LOG_DEBUG("USBHUB: state_extra_disabled");

    if constexpr ( ENABLE_RGB_LED && RGB_DISABLE_DURING_USB_SUSPEND )
        if ( !main_thread::is_dfu_mode() )
            rgb_gcr::enable();
}

void state_extra_disabled::isr_process_v_con_report()
{
    if ( v_extra->is_device_connected() ) {
        if ( !m_panic_disabled ) {
            static event_t _event = { nullptr,  // .list_node
                [](event_t*) {  // .handler
                    LOG_INFO("USBHUB: extra device is connected to port %d",
                        v_extra->line);
                    transition_to<state_extra_enabled>();
                }
            };

            usbhub_thread::signal_event(&_event);
        }
    }
    else {
        if ( m_panic_disabled ) {
            m_panic_disabled = false;
            LOG_INFO("USBHUB: extra device is disconnected from port %d", v_extra->line);
        }
    }
}

void state_extra_disabled::process_extra_enable_manually()
{
    transition_to<state_extra_enabled>().process_extra_enable_manually();
}



void state_extra_enabled::begin()
{
    usbhub_enable_extra_port(v_extra->line, true);
    LOG_DEBUG("USBHUB: state_extra_enabled");

    if constexpr ( ENABLE_RGB_LED && RGB_DISABLE_DURING_USB_SUSPEND )
        if ( !main_thread::is_dfu_mode() )
            rgb_gcr::enable();
}

void state_extra_enabled::isr_process_v_5v_report()
{
    if ( ztimer_is_set(ZTIMER_MSEC, &extra_cutting_timer) ) {
        if ( adc::v_5v.level() >= adc_v_5v::STABLE )
            ztimer_remove(ZTIMER_MSEC, &extra_cutting_timer);
    }
    else {
        if ( adc::v_5v.level() < adc_v_5v::STABLE )
            ztimer_set_timeout_flag(ZTIMER_MSEC,
                &extra_cutting_timer, GRACE_TIME_TO_CUT_EXTRA_MS);
    }
}

void state_extra_enabled::isr_process_v_con_report()
{
    if ( !v_extra->is_device_connected() ) {
        if ( !m_enabled_manually ) {
            static event_t _event = { nullptr,  // .list_node
                [](event_t*) {  // .handler
                    LOG_INFO("USBHUB: extra device is disconnected from port %d",
                        v_extra->line);
                    transition_to<state_extra_disabled>();
                }
            };

            usbhub_thread::signal_event(&_event);
        }
    }
}

void state_extra_enabled::process_extra_enable_manually()
{
    if ( !m_enabled_manually ) {
        m_enabled_manually = true;
        LOG_INFO("USBHUB: extra port is enabled manually");
    }
}

void state_extra_enabled::process_extra_enable_automatically()
{
    if ( m_enabled_manually ) {
        m_enabled_manually = false;
        LOG_INFO("USBHUB: extra port is back to automatic");
        if ( !v_extra->is_device_connected() )
            transition_to<state_extra_disabled>();
    }
}

void state_extra_enabled::process_timeout()
{
    LOG_WARNING("USBHUB: extra port is panic disabled");
    transition_to<state_extra_disabled>().set_panic_disabled();
}

void state_extra_enabled::end()
{
    ztimer_remove(ZTIMER_MSEC, &extra_cutting_timer);
    m_enabled_manually = false;
    if constexpr ( !KEEP_CHARGING_EXTRA_DEVICE_DURING_SUSPEND )
        usbhub_enable_extra_port(v_extra->line, false);
}
