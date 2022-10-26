#include "usb2422.h"

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "adc_input.hpp"
#include "usbport_state.hpp"



usbport* usbport::pstate;
adc_input_v_con* usbport::v_extra;

void usbport::init_state()
{
    pstate = &state_determine_host::obj();
    pstate->begin();
    v_extra = nullptr;
}

void state_determine_host::process_timeout()
{
    // Blink LED0 to indicate that we're alive. We can stay in this state when plugged in
    // to a suspended host, until the host wakes up.
    LED0_TOGGLE;

    usbhub_disable_all_ports();

    if ( desired_port == USB_PORT_UNKNOWN )
        // Determine the host by comparing measurements on v_con1 and v_con2.
        desired_port = adc_input::measure_host_port();
    usbhub_enable_host_port(desired_port);

    // If the desired port couldn't be acquired first, then try either port that has a
    // higher level afterwards.
    desired_port = USB_PORT_UNKNOWN;
}

void state_determine_host::begin() {
    // The desired_port will be USB_PORT_UNKNOWN at the first entry since sr_exp_init()
    // is called in board_init(). Or, it will retain the extra port that was left over
    // by the last state.
    desired_port = usbhub_extra_port();

    if ( v_extra != nullptr ) {
        v_extra->schedule_cancel();
        DEBUG("ADC: switchover to port %d\n", desired_port);  // == v_extra->line
        v_extra = nullptr;
    }

    // Set up periodic retry on every FLAG_TIMEOUT.
    retry_timer.start(true);  // true == expire it now.
}

void state_determine_host::end() {
    retry_timer.stop();

    v_extra = (usbhub_extra_port() == USB_PORT_1)
            ? &adc_input::v_con1 : &adc_input::v_con2;
    v_extra->schedule_periodic();

    LED0_OFF;  // Turn off the blinking LED0.
    DEBUG("ADC: determined host @port %d\n", usbhub_host_port());
}

void state_extra_disabled::process_extra_connected() {
    usbhub_enable_extra_port(v_extra->line, true);
    DEBUG("ADC: extra device is connected to port %d\n", v_extra->line);
    transition_to<state_extra_enabled>();
}

void state_extra_disabled::process_extra_enable_manually() {
    usbhub_enable_extra_port(v_extra->line, true);
    DEBUG("ADC: extra port is enabled manually @port %d\n", v_extra->line);
    transition_to<state_extra_enabled>(&usbport::process_extra_enable_manually);
}

void state_extra_enabled::process_extra_unconnected() {
    if ( !enabled_manually ) {
        usbhub_enable_extra_port(v_extra->line, false);
        DEBUG("ADC: extra device is disconnected from port %d\n", v_extra->line);
        transition_to<state_extra_disabled>();
    }
}

void state_extra_enabled::process_extra_enable_manually() {
    if ( !enabled_manually ) {
        enabled_manually = true;
        DEBUG("ADC: extra port is enabled manually @port %d\n", v_extra->line);
    }
}

void state_extra_enabled::process_extra_back_to_automatic() {
    if ( enabled_manually ) {
        enabled_manually = false;
        DEBUG("ADC: extra port is back to automatic control @port %d\n", v_extra->line);
    }
}

void state_extra_enabled::process_v_5v_level() {
    // Todo: GCR adjusting should do its best instead of wasting the timer for nothing.
    if ( adc_input::v_5v.level() < adc_input_v_5v::V_5V_STABLE ) {
        if ( !extra_cutting_timer.is_running() )
            extra_cutting_timer.start();
    } else {
        extra_cutting_timer.stop();
    }
}

void state_extra_enabled::process_timeout() {
    usbhub_enable_extra_port(v_extra->line, false);
    DEBUG("ADC: extra port is panic disabled @port %d\n", v_extra->line);
    transition_to<state_extra_panic_disabled>();
}

void state_extra_enabled::end() {
    extra_cutting_timer.stop();
    // Set it back to automatic before transitioning out.
    enabled_manually = false;
}

void state_extra_panic_disabled::process_extra_unconnected() {
    // Todo: Also ensure that v_5v level is back to V_5V_STABLE.
    DEBUG("ADC: extra device is disconnected from port %d\n", v_extra->line);
    transition_to<state_extra_disabled>();
}
