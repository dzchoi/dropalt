#pragma once

#include "ztimer.h"             // for ztimer_t



class adc_input_v_con;
class state_usb_suspended;
class state_extra_disabled;
class state_extra_enabled;



// This class is more like a namespace than a class, though also serves as the base class
// of all state_* child classes. Be careful of defining non-static data members as they
// will be inherited by all child classes.
class usbport {
public:
    static inline usbport* pstate = nullptr;

    // Note that v_host is measured with SR_CTRL_SRC_x enabled while v_extra is measured
    // with SR_CTRL_SRC_y disabled, and v_extra is kept measuring periodically while
    // v_host is not and has to be measured explicitly.
    static inline adc_input_v_con* v_host = nullptr;   // either v_con1 or v_con2
    static inline adc_input_v_con* v_extra = nullptr;  // the opposite port of v_host

    static void setup_state();  // Set up initial state.

    // Event handlers
    // Note that the events triggered from measurements are recurring periodically
    // (process_v_5v_level, process_extra_connected and process_extra_unconnected) but
    // the others are not. If an event is not processed by the current state it is lost
    // and not passed over to the next state (see adc_thread::_adc_thread).
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

    template <class State>
    static State& jump_to()
    {
        State& next_state = State::obj();
        pstate = &next_state;
        pstate->begin();
        return next_state;
    }

    template <class State>
    static State& transit_to()
    {
        pstate->end();
        return jump_to<State>();
    }

    void help_process_usb_suspend();
    void help_process_usb_resume();
    void help_process_usbport_switchover();
};



class state_usb_suspended: public usbport {
// Will stay until resumed, or until switchover or remote wake-up is triggered.
public:
    static state_usb_suspended& obj(){
        static state_usb_suspended obj;
        return obj;
    }

    // Determine and acquire host port during power-up or switchover.
    void determine_host();

    void process_usb_resume() { help_process_usb_resume(); }
    void process_usbport_switchover() { help_process_usbport_switchover(); }
    void process_timeout();

private:
    void begin();
    void end();

    ztimer_t blink_timer = {};
};



class state_extra_disabled: public usbport {
public:
    static state_extra_disabled& obj() {
        static state_extra_disabled obj;
        return obj;
    }

    // Once the extra port is disabled due to panic it remains disabled until the extra
    // device is plugged out, USB is suspended, or the extra port is enabled manually
    // (though it may result in the panic disabling again).
    void set_panic_disabled() { m_panic_disabled = true; }

    void process_usb_suspend() { help_process_usb_suspend(); }
    void process_usbport_switchover() { help_process_usbport_switchover(); }
    void process_extra_connected();
    void process_extra_unconnected();
    void process_extra_enable_manually();

private:
    void begin();
    void end() { m_panic_disabled = false; }

    bool m_panic_disabled = false;
};



class state_extra_enabled: public usbport {
public:
    static state_extra_enabled& obj() {
        static state_extra_enabled obj;
        return obj;
    }

    void process_usb_suspend() { help_process_usb_suspend(); }
    // Switchover is allowed but will be rejected by help_process_usbport_switchover().
    void process_usbport_switchover() { help_process_usbport_switchover(); }
    void process_extra_unconnected();
    void process_extra_enable_manually();
    void process_extra_back_to_automatic();
    void process_v_5v_level();
    void process_timeout();

private:
    void begin();
    void end();

    bool m_enabled_manually = false;

    ztimer_t extra_cutting_timer = {};
};
