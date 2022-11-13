#pragma once

#include "features.hpp"         // for LED_BLINK_PERIOD_DURING_SUSPEND



class adc_input_v_con;
class state_determine_host;
class state_usb_suspended;
class state_extra_disabled;
class state_extra_enabled;
class state_extra_panic_disabled;



// This class is more like a namespace than a class, though also serves as the base class
// of all state_* child classes. Be careful of defining non-static data members as they
// will be inherited by all child classes.
class usbport {
public:
    static usbport* pstate;

    // Note that v_extra is available in all states but state_determine_host, and
    // process_extra_connected() and process_extra_unconnected() can be called only in
    // those states as they are based on v_extra->schedule_periodic().
    static adc_input_v_con* v_extra;  // either v_con1 or v_con2

    static void init_state();  // Set up the initial state.

    // Event processing methods
    virtual void process_usb_suspend() {}
    virtual void process_usb_resume() {}
    virtual void process_usbport_switchover() {}
    virtual void process_extra_connected() {}
    virtual void process_extra_unconnected() {}
    virtual void process_extra_enable_manually() {}
    virtual void process_extra_back_to_automatic() {}
    virtual void process_v_5v_level() {}
    virtual void process_timeout() {}

protected:
    usbport() =default;  // Can be called only by child classes.

    virtual void begin() {}
    virtual void end() {}

    template <class T>
    static T& transition_to()
    {
        pstate->end();
        T& next_state = T::obj();
        pstate = &next_state;
        pstate->begin();
        return next_state;
    }
};



class state_determine_host: public usbport {
// Determine the host port between USB_PORT_1 and USB_PORT_2.
// Todo: If the USB in the desired port is suspended after switchover, wake it up first.
public:
    static state_determine_host& obj() {
        static state_determine_host obj;
        return obj;
    }

    void process_usb_resume() { transition_to<state_extra_disabled>(); }
    void process_timeout();

private:
    void begin();
    void end();

    uint8_t desired_port;
#if ENABLE_DEBUG
    uint32_t since;
#endif

    // Normally it will take 600-700 ms to acquire the host port.
    static constexpr uint32_t RETRY_PERIOD_US = 1 *US_PER_SEC;
    xtimer_periodic_signal_t retry_timer { RETRY_PERIOD_US };
};



class state_usb_suspended: public usbport {
public:
    static state_usb_suspended& obj(){
        static state_usb_suspended obj;
        return obj;
    }

    void process_usb_resume() { transition_to<state_extra_disabled>(); }
    void process_usbport_switchover() { transition_to<state_determine_host>(); }
    void process_timeout() { LED0_TOGGLE; }

private:
    void begin() { blink_timer.start(true); }
    void end() { blink_timer.stop(); LED0_OFF; }

    xtimer_periodic_signal_t blink_timer { LED_BLINK_PERIOD_DURING_SUSPEND };
};



class state_extra_disabled: public usbport {
public:
    static state_extra_disabled& obj() {
        static state_extra_disabled obj;
        return obj;
    }

    void process_usb_suspend() { transition_to<state_usb_suspended>(); }
    void process_usbport_switchover() { transition_to<state_determine_host>(); }
    void process_extra_connected();
    void process_extra_enable_manually();
};



class state_extra_enabled: public usbport {
public:
    static state_extra_enabled& obj() {
        static state_extra_enabled obj;
        return obj;
    }

    void process_usb_suspend() { transition_to<state_usb_suspended>(); }
    void process_usbport_switchover() { transition_to<state_determine_host>(); }
    void process_extra_unconnected();
    void process_extra_enable_manually();
    void process_extra_back_to_automatic();
    void process_v_5v_level();
    void process_timeout();

private:
    void end();

    bool enabled_manually = false;

    static constexpr uint32_t EXTRA_CUTTING_GRACE_TIME_MS = 1000 *MS_PER_SEC;
    xtimer_onetime_signal_t extra_cutting_timer { EXTRA_CUTTING_GRACE_TIME_MS };
};



class state_extra_panic_disabled: public usbport {
public:
    static state_extra_panic_disabled& obj() {
        static state_extra_panic_disabled obj;
        return obj;
    }

    void process_usb_suspend() { transition_to<state_usb_suspended>(); }
    void process_usbport_switchover() { transition_to<state_determine_host>(); }
    void process_extra_unconnected();
};
