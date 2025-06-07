#pragma once

#include "ztimer.h"             // for ztimer_t



class adc_v_con;

// This self-contained class acts more like a namespace than a class, though it acts as
// the base class of all child state_ classes. Note that the static data members are not
// inherited but shared among all derived classes.
class usbhub_state {
public:
    // `pstate` can point to any derived state_ class of usbhub_state.
    static inline usbhub_state* pstate = nullptr;

    // Note that v_host is measured with SR_CTRL_SRC_x enabled while v_extra is measured
    // with SR_CTRL_SRC_x disabled, and v_extra is measured automatically and periodically
    // but v_host should be measured explicitly.
    static inline adc_v_con* v_host = nullptr;   // can be either v_con1 or v_con2.
    static inline adc_v_con* v_extra = nullptr;  // the opposite port of v_host

    static void init();  // Set up initial state.

    // Event handlers
    // Note that events triggered by ADC measurements (process_v_5v_report() and
    // process_v_con_report()) occur periodically, while other events do not. If an event
    // is not handled by the current state, it is discarded and not forwarded to the next
    // state. (See usbhub_thread::_thread_entry().)
    virtual void process_usb_suspend() {}
    virtual void process_usb_resume() {}
    virtual void process_usbport_switchover() {}
    virtual void process_v_5v_report() {}
    virtual void process_v_con_report() {}
    virtual void process_extra_enable_manually() {}
    virtual void process_extra_enable_automatically() {}
    virtual void process_timeout() {}

protected:
    // Unlikely but if the keyboard's USB port is physically damaged, the ADC measurement
    // on the port can be incorrect and the automatic switchover on cable break will not
    // work.
    static inline bool automatic_switchover_enabled = false;

    usbhub_state() =default;  // can be called only by child classes.

    // Invoked when entering or exiting a state.
    virtual void begin() {}
    virtual void end() {}

    // Be careful to not access any class members in a method of state_ class after
    // calling transition_to(), because calling pstate->transition_to<NextState>() will
    // change pstate itself (i.e. `this`). Though modifying pstate from transition_to()
    // would not affect `this` in the method, as `this` is compiled to be a parameter
    // when calling the method, we would still better call transition_to() at the end of
    // the method.
    template <class NextState>
    static NextState& transition_to()
    {
        pstate->end();
        return jump_to<NextState>();
    }

    // `jump_to()` is similar to `transit_to()`, only it does not call `end()` on the
    // current state.
    template <class NextState>
    static NextState& jump_to()
    {
        NextState& next_state = NextState::obj();
        pstate = &next_state;
        pstate->begin();
        return next_state;
    }

    void help_process_usb_suspend();
    void help_process_usb_resume();
    void help_process_usbport_switchover();
};



class state_determine_host: public usbhub_state {
// Determine host port during power-up.
// When powering up with USB cable plugged in, it usually takes a few hundred milliseconds
// to acquire the host (USB resumes).
public:
    static state_determine_host& obj() {
        static state_determine_host obj;
        return obj;
    }

    void process_usb_resume() { help_process_usb_resume(); }
    void process_usbport_switchover() { help_process_usbport_switchover(); }
    void process_v_con_report();
    void process_timeout();

private:
    void begin();
    void end();

    ztimer_t retry_timer;
};



class state_usb_suspend: public usbhub_state {
// Will stay until USB is resumed, or until a switchover or remote wake-up is triggered.
public:
    static state_usb_suspend& obj(){
        static state_usb_suspend obj;
        return obj;
    }

    // Acquire host port on switchover.
    void perform_switchover();

    void process_usb_resume() { help_process_usb_resume(); }
    void process_usbport_switchover() { help_process_usbport_switchover(); }
    void process_timeout();

private:
    void begin();
    void end();

    ztimer_t blink_timer;
};



class state_extra_disabled: public usbhub_state {
// USB is connected (resumed) but the extra port is disabled.
public:
    static state_extra_disabled& obj() {
        static state_extra_disabled obj;
        return obj;
    }

    // Once the extra port is disabled due to panic it remains disabled until the extra
    // device is plugged out, USB is suspended, or the extra port is enabled manually
    // (though in this case it may end up being disabled again in panic).
    void set_panic_disabled() { m_panic_disabled = true; }

    void process_usb_suspend() { help_process_usb_suspend(); }
    void process_usbport_switchover() { help_process_usbport_switchover(); }
    void process_v_con_report();
    void process_extra_enable_manually();

private:
    void begin();
    void end() { m_panic_disabled = false; }

    bool m_panic_disabled = false;
};



class state_extra_enabled: public usbhub_state {
// USB is connected (resumed) and the extra port is enabled for a device.
public:
    static state_extra_enabled& obj() {
        static state_extra_enabled obj;
        return obj;
    }

    void process_usb_suspend() { help_process_usb_suspend(); }
    // Switchover is allowed but will be rejected by help_process_usbport_switchover().
    void process_usbport_switchover() { help_process_usbport_switchover(); }
    void process_v_5v_report();
    void process_v_con_report();
    void process_extra_enable_manually();
    void process_extra_enable_automatically();
    void process_timeout();

private:
    void begin();
    void end();

    bool m_enabled_manually = false;

    ztimer_t extra_cutting_timer;
};
