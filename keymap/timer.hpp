#pragma once

#include "ztimer.h"

#include "event_ext.hpp"        // for event_ext_t



namespace key {

class pmap_t;



class timer_t: public ztimer_t {
protected: // Methods to be used by child classes
    constexpr timer_t(uint32_t timeout_ms): ztimer_t(), m_timeout_ms(timeout_ms) {
        callback = _tmo_key_timer;
    }

    void start_timer(pmap_t* slot);

    void stop_timer();

    bool timer_is_running() const { return ztimer_is_set(ZTIMER_MSEC, this); }

    pmap_t* get_slot() const { return m_timeout_expected; }

    ~timer_t() { stop_timer(); }

    timer_t(const timer_t&) =delete;
    void operator=(const timer_t&) =delete;

private: // Methods to be called by keymap_thread and key::manager
    friend class ::keymap_thread;  // allows access to handle_timeout()
    friend class manager_t;

    // Will be called when the timer is expired.
    virtual void on_timeout(pmap_t*) =0;

    // Inherent problem with stopping a timer is that it is not an atomic operation and
    // we can still have a "spurious" timeout interrupt before/during the operation. To
    // deal with the problem, after a timeout interrupt occurred, we can check if it is
    // legitimate and expected timeout, discarding it if not.
    pmap_t* timeout_expected();

    event_ext_t<timer_t*> m_event_timeout = { nullptr, _hdlr_timeout, this };
    // _hdlr_timeout() is executed in the context of keymap_thread.
    static void _hdlr_timeout(event_t* event);

    static void _tmo_key_timer(void* arg);

    const uint32_t m_timeout_ms;
    pmap_t* m_timeout_expected = nullptr;
};

}  // namespace key
