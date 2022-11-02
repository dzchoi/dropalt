#pragma once

class adc_input;
class state_determine_host;
class state_extra_disabled;
class state_extra_enabled;
class state_extra_panic_disabled;



class usbport {
    // This class is more like a namespace than a class, though also serves the base
    // class of all state_* classes. Be careful of defining a non-static data member as
    // it will be inherited by child classes.
public:
    static usbport* pstate;

    static adc_input_v_con* v_extra;  // either v_con1 or v_con2

    static void init_state();  // Set up the initial state.

    // Note that v_extra is available in all states but state_determine_host, and
    // process_extra_connected() and process_extra_unconnected() can be called only in
    // those states as they are based on v_extra->schedule_periodic().

    virtual void process_usbhub_active() {}
    virtual void process_usbhub_switchover() {}
    virtual void process_extra_connected() {}
    virtual void process_extra_unconnected() {}
    virtual void process_extra_enable_manually() {}
    virtual void process_extra_back_to_automatic() {}
    virtual void process_v_5v_level() {}
    virtual void process_timeout() {}

protected:
    virtual void begin() {}
    virtual void end() {}

    template <class T>
    static void transition_to() {
        pstate->end();
        pstate = &T::obj();
        pstate->begin();
    }

    template <class T>
    static void transition_to(void (usbport::*process_at_entry)()) {
        transition_to<T>();
        (pstate->*process_at_entry)();
    }

    usbport() {}  // Can be called only by child classes.
};



class state_determine_host: public usbport {
// Determine the host port between USB_PORT_1 and USB_PORT_2.
// Todo: If the USB in the desired port is suspended when switchover, wake it up first.
public:
    static state_determine_host& obj() {
        static state_determine_host obj;
        return obj;
    }

    void process_usbhub_active() { transition_to<state_extra_disabled>(); }
    void process_timeout();

private:
    void begin();
    void end();

    uint8_t desired_port;

    static constexpr uint32_t RETRY_PERIOD = 1 *US_PER_SEC;
    xtimer_periodic_signal_t retry_timer { RETRY_PERIOD };
};



class state_extra_disabled: public usbport {
public:
    static state_extra_disabled& obj() {
        static state_extra_disabled obj;
        return obj;
    }

    void process_usbhub_switchover() { transition_to<state_determine_host>(); }
    void process_extra_connected();
    void process_extra_enable_manually();
};



class state_extra_enabled: public usbport {
public:
    static state_extra_enabled& obj() {
        static state_extra_enabled obj;
        return obj;
    }

    void process_usbhub_switchover() { transition_to<state_determine_host>(); }
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

    void process_usbhub_switchover() { transition_to<state_determine_host>(); }
    void process_extra_unconnected();
};
