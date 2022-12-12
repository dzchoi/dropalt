#include "usb2422.h"
#include "ztimer.h"             // for ztimer_sleep(), ztimer_set_timeout_flag(), ...

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "adc_input.hpp"        // for v_5v, v_con1 and v_con2
#include "features.hpp"         // for DEBUG_LED_BLINK_PERIOD_MS
#include "usbport_states.hpp"



void usbport::setup_state()
{
    jump_to<state_usb_suspended>().determine_host();
}

void usbport::help_process_usb_suspend()
{
    if ( v_host->sync_measure().is_host_connected() )
        transit_to<state_usb_suspended>();
    else
        // Automatic switchover upon cable break.
        transit_to<state_usb_suspended>().determine_host();
}

void usbport::help_process_usb_resume()
{
    // Normally it will take 600-900 ms to acquire host port at power-up.
    DEBUG("ADC: acquired host @port %d\n", usbhub_host_port());

    if ( v_extra->is_device_connected() )
        transit_to<state_extra_enabled>();
    else
        transit_to<state_extra_disabled>();
}

void usbport::help_process_usbport_switchover()
{
    // We could allow switchover only when v_extra->is_host_connected(), but we would
    // better let users decide and be more permissive (e.g. switchover to an unconnected
    // port.)
    if ( !v_extra->is_device_connected() )
        transit_to<state_usb_suspended>().determine_host();
    else
        DEBUG("ADC:\e[0;31m switchover not allowed to extra device!\e[0m\n");
}

void state_usb_suspended::determine_host()
{
    if ( v_extra )
        v_extra->schedule_cancel();

    // Disconnect the current host; disabling upstream mux (setting SR_CTRL_E_UP_N = 1)
    // or selecting different host (changing SR_CTRL_S_UP) will disconnect the current
    // host, and once disconnected the remote wake-up will likely no longer work when
    // we switch back to it. After the disconnection FLAG_USB_SUSPEND (thus
    // process_usb_suspend()) will follow immediately but it is ignored in this state.
    // It also disconnects the data line (SR_CTRL_E_DN1_N) of the extra port but the
    // power line (SR_CTRL_E_VBUS_x) is intact.
    usbhub_disable_all_ports();

    uint8_t desired_port = USB_PORT_UNKNOWN;
    if ( v_extra ) {
        desired_port = v_extra->line;  // == usbhub_extra_port();
        DEBUG("ADC: switchover to port %d\n", desired_port);
    }
    else {
        // Determine the host port at power-up. This may loop for a few hundred
        // milliseconds when powering up the keyboard by plugging in the USB cable.
        while ( true ) {
            if constexpr ( POWER_UP_CHECK_PORT1_FIRST ) {
                if ( adc_input::v_con1.sync_measure().is_host_connected() )
                    desired_port = USB_PORT_1;
                else if ( adc_input::v_con2.sync_measure().is_host_connected() )
                    desired_port = USB_PORT_2;
            } else {
                if ( adc_input::v_con2.sync_measure().is_host_connected() )
                    desired_port = USB_PORT_2;
                else if ( adc_input::v_con1.sync_measure().is_host_connected() )
                    desired_port = USB_PORT_1;
            }

            if ( desired_port != USB_PORT_UNKNOWN )
                break;
            ztimer_sleep(ZTIMER_MSEC, 10);  // So, each iteration will take 12 ms.
        }

        DEBUG("ADC: v_con1=%d v_con2=%d\n",
            adc_input::v_con1.read(), adc_input::v_con2.read());
    }

    usbhub_enable_host_port(desired_port);

    if ( desired_port == USB_PORT_1 ) {
        v_host  = &adc_input::v_con1;
        v_extra = &adc_input::v_con2;
    } else {
        v_host  = &adc_input::v_con2;
        v_extra = &adc_input::v_con1;
    }

    // From now on, v_extra will be measured periodically.
    v_extra->schedule_periodic();
}

void state_usb_suspended::process_timeout()
{
    LED0_TOGGLE;
    ztimer_set_timeout_flag(ZTIMER_MSEC, &blink_timer, DEBUG_LED_BLINK_PERIOD_MS);
}

void state_usb_suspended::begin()
{
    DEBUG("ADC:\e[0;34m state_usb_suspended\e[0m\n");
    process_timeout();  // Expire blink_timer now!
}

void state_usb_suspended::end()
{
    ztimer_remove(ZTIMER_MSEC, &blink_timer);
    LED0_OFF;
}

void state_extra_disabled::process_extra_connected()
{
    if ( !m_panic_disabled ) {
        DEBUG("ADC: extra device is connected @port %d\n", v_extra->line);
        transit_to<state_extra_enabled>();
    }
}

void state_extra_disabled::process_extra_unconnected()
{
    if ( m_panic_disabled ) {
        m_panic_disabled = false;
        DEBUG("ADC: extra device is disconnected from port %d\n", v_extra->line);
    }
}

void state_extra_disabled::process_extra_enable_manually()
{
    transit_to<state_extra_enabled>().process_extra_enable_manually();
}

void state_extra_disabled::begin()
{
    usbhub_switch_enable_extra_port(v_extra->line, false);
    DEBUG("ADC:\e[0;34m state_extra_disabled\e[0m\n");
}

void state_extra_enabled::process_extra_unconnected()
{
    if ( !m_enabled_manually ) {
        DEBUG("ADC: extra device is disconnected from port %d\n", v_extra->line);
        transit_to<state_extra_disabled>();
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

void state_extra_enabled::process_v_5v_level()
{
    // Todo: GCR adjusting should do its best while running the timer.
    if ( adc_input::v_5v.level() < adc_input_v_5v::V_5V_STABLE ) {
        if ( !ztimer_is_set(ZTIMER_MSEC, &extra_cutting_timer) )
            ztimer_set_timeout_flag(ZTIMER_MSEC,
                &extra_cutting_timer, GRACE_TIME_TO_CUT_EXTRA_MS);
    }
    else
        ztimer_remove(ZTIMER_MSEC, &extra_cutting_timer);
}

void state_extra_enabled::process_timeout()
{
    DEBUG("ADC:\e[0;31m extra port is panic disabled\e[0m\n");
    transit_to<state_extra_disabled>().set_panic_disabled();
}

void state_extra_enabled::begin()
{
    usbhub_switch_enable_extra_port(v_extra->line, true);
    DEBUG("ADC:\e[0;34m state_extra_enabled\e[0m\n");
}

void state_extra_enabled::end()
{
    ztimer_remove(ZTIMER_MSEC, &extra_cutting_timer);
    m_enabled_manually = false;
    if constexpr ( !KEEP_CHARGING_EXTRA_DEVICE_DURING_SUSPEND )
        usbhub_switch_enable_extra_port(v_extra->line, false);
}
