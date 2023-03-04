#include "usb2422.h"
#include "ztimer.h"             // for ztimer_set_timeout_flag(), ztimer_remove(), ...

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include <utility>              // for std::swap()
#include "adc_input.hpp"        // for v_5v, v_con1/con2, ...
#include "features.hpp"         // for DEBUG_LED_BLINK_PERIOD_MS, ...
#include "persistent.hpp"       // for persistent::obj()
#include "usbport_states.hpp"



void usbport::setup_state()
{
    jump_to<state_determine_host>();
}

void usbport::help_process_usb_suspend()
{
    if ( enable_automatic_switchover && !v_host->sync_measure().is_host_connected() )
        // Automatic switchover upon cable break.
        transit_to<state_usb_suspended>().perform_switchover();
    else
        transit_to<state_usb_suspended>();
}

void usbport::help_process_usb_resume()
{
    DEBUG("ADC: acquired host port %d @%lu\n", v_host->line, ztimer_now(ZTIMER_MSEC));

    if ( v_extra->sync_measure().is_device_connected() )
        transit_to<state_extra_enabled>();
    else
        transit_to<state_extra_disabled>();
}

void usbport::help_process_usbport_switchover()
{
    // We could allow switchover only when v_extra->is_host_connected(), but it would
    // better be more permissive and let users decide (e.g. switchover to an unconnected
    // port.)
    if ( !v_extra->sync_measure().is_device_connected() )
        transit_to<state_usb_suspended>().perform_switchover();
    else
        DEBUG("ADC:\e[0;31m switchover not allowed to extra device!\e[0m\n");
}



void state_determine_host::begin()
{
    LED0_ON;
    DEBUG("ADC:\e[0;34m state_determine_host\e[0m\n");

    if ( v_extra )
        v_extra->schedule_cancel();

    // Disconnect the current host.
    // Be aware that disabling upstream mux (setting SR_CTRL_E_UP_N = 1) or selecting a
    // different host (changing SR_CTRL_S_UP) will disconnect the current host, and once
    // disconnected a remote wake-up will likely no longer work when we switch back to it.
    // After the disconnection the FLAG_USB_SUSPEND event (hence process_usb_suspend())
    // will follow immediately but it is ignored in state_determine_host state. It also
    // disconnects the data line (SR_CTRL_E_DN1_N) of the extra port but the power line
    // (SR_CTRL_E_VBUS_x) is intact.
    usbhub_disable_all_ports();

    const uint8_t desired_port = persistent::obj().last_host_port;
    DEBUG("ADC: try port %d first @%lu\n", desired_port, ztimer_now(ZTIMER_MSEC));

    usbhub_enable_host_port(desired_port);
    if ( desired_port == USB_PORT_1 ) {
        v_host  = &adc_input::v_con1;
        v_extra = &adc_input::v_con2;
    } else {
        v_host  = &adc_input::v_con2;
        v_extra = &adc_input::v_con1;
    }

    // Note that state_determine_host measures v_host (during power-up) while the other
    // states measure v_extra.
    v_host->schedule_periodic();

    ztimer_set_timeout_flag(ZTIMER_MSEC, &blink_timer, DEBUG_LED_BLINK_PERIOD_MS);
}

void state_determine_host::process_v_con()
{
    if ( v_host->is_host_connected() ) {
        v_host->schedule_cancel();
        DEBUG("ADC: determined host port %d @%lu\n",
            v_host->line, ztimer_now(ZTIMER_MSEC));

        // Remember the host port when acquired only from state_determine_host.
        persistent::obj().write(&persistent::last_host_port, v_host->line);
    }
}

void state_determine_host::process_timeout()
{
    LED0_TOGGLE;
    v_host->schedule_cancel();

    const uint8_t desired_port = v_extra->line;  // == usbhub_extra_port();
    DEBUG("ADC: switchover to port %d @%lu\n", desired_port, ztimer_now(ZTIMER_MSEC));

    usbhub_enable_host_port(desired_port);
    std::swap(v_host, v_extra);

    v_host->schedule_periodic();
    ztimer_set_timeout_flag(ZTIMER_MSEC, &blink_timer, DEBUG_LED_BLINK_PERIOD_MS);
}

void state_determine_host::end()
{
    LED0_OFF;
    ztimer_remove(ZTIMER_MSEC, &blink_timer);
    v_host->schedule_cancel();

    enable_automatic_switchover = v_host->is_host_connected();
    if ( !enable_automatic_switchover )
        DEBUG("ADC:\e[0;33m automatic switchover disabled\e[0m\n");

    // From now on, v_extra will be measured periodically.
    v_extra->schedule_periodic();
}



void state_usb_suspended::begin()
{
    LED0_ON;
    DEBUG("ADC:\e[0;34m state_usb_suspended\e[0m\n");

    // v_extra is not measured in state_usb_suspended.
    if ( v_extra )
        v_extra->schedule_cancel();

    ztimer_set_timeout_flag(ZTIMER_MSEC, &blink_timer, DEBUG_LED_BLINK_PERIOD_MS);
}

void state_usb_suspended::perform_switchover()
{
    assert( v_host != nullptr && v_extra != nullptr );

    const uint8_t desired_port = v_extra->line;  // == usbhub_extra_port();
    DEBUG("ADC: switchover to port %d @%lu\n", desired_port, ztimer_now(ZTIMER_MSEC));

    usbhub_enable_host_port(desired_port);
    std::swap(v_host, v_extra);
}

void state_usb_suspended::process_timeout()
{
    LED0_TOGGLE;
    ztimer_set_timeout_flag(ZTIMER_MSEC, &blink_timer, DEBUG_LED_BLINK_PERIOD_MS);
}

void state_usb_suspended::end()
{
    LED0_OFF;
    ztimer_remove(ZTIMER_MSEC, &blink_timer);

    enable_automatic_switchover = v_host->sync_measure().is_host_connected();
    if ( !enable_automatic_switchover )
        DEBUG("ADC:\e[0;33m automatic switchover disabled\e[0m\n");

    // From now on, v_extra will be measured periodically.
    v_extra->schedule_periodic();
}



void state_extra_disabled::begin()
{
    usbhub_switch_enable_extra_port(v_extra->line, false);
    DEBUG("ADC:\e[0;34m state_extra_disabled\e[0m\n");
}

void state_extra_disabled::process_v_con()
{
    if ( v_extra->is_device_connected() ) {
        if ( !m_panic_disabled ) {
            DEBUG("ADC: extra device is connected to port %d\n", v_extra->line);
            transit_to<state_extra_enabled>();
        }
    }
    else {
        if ( m_panic_disabled ) {
            m_panic_disabled = false;
            DEBUG("ADC: extra device is disconnected from port %d\n", v_extra->line);
        }
    }
}

void state_extra_disabled::process_extra_enable_manually()
{
    transit_to<state_extra_enabled>().process_extra_enable_manually();
}



void state_extra_enabled::begin()
{
    usbhub_switch_enable_extra_port(v_extra->line, true);
    DEBUG("ADC:\e[0;34m state_extra_enabled\e[0m\n");
}

void state_extra_enabled::process_v_5v()
{
    if ( ztimer_is_set(ZTIMER_MSEC, &extra_cutting_timer) ) {
        if ( adc_input::v_5v.level() >= V_5V_STABLE )
            ztimer_remove(ZTIMER_MSEC, &extra_cutting_timer);
    }
    else {
        if ( adc_input::v_5v.level() < V_5V_STABLE )
            ztimer_set_timeout_flag(ZTIMER_MSEC,
                &extra_cutting_timer, GRACE_TIME_TO_CUT_EXTRA_MS);
    }
}

void state_extra_enabled::process_v_con()
{
    if ( !v_extra->is_device_connected() ) {
        if ( !m_enabled_manually ) {
            DEBUG("ADC: extra device is disconnected from port %d\n", v_extra->line);
            transit_to<state_extra_disabled>();
        }
    }
}

void state_extra_enabled::process_extra_enable_manually()
{
    if ( !m_enabled_manually ) {
        m_enabled_manually = true;
        DEBUG("ADC: extra port is enabled manually\n");
    }
}

void state_extra_enabled::process_extra_back_to_automatic()
{
    if ( m_enabled_manually ) {
        m_enabled_manually = false;
        DEBUG("ADC: extra port is back to automatic\n");
        if ( !v_extra->is_device_connected() )
            transit_to<state_extra_disabled>();
    }
}

void state_extra_enabled::process_timeout()
{
    DEBUG("ADC:\e[0;31m extra port is panic disabled\e[0m\n");
    transit_to<state_extra_disabled>().set_panic_disabled();
}

void state_extra_enabled::end()
{
    ztimer_remove(ZTIMER_MSEC, &extra_cutting_timer);
    m_enabled_manually = false;
    if constexpr ( !KEEP_CHARGING_EXTRA_DEVICE_DURING_SUSPEND )
        usbhub_switch_enable_extra_port(v_extra->line, false);
}
