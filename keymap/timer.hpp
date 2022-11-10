#pragma once

#include "xtimer.h"



namespace key {

class pmap_t;



class timer_t: public xtimer_t {
protected: // Methods to be used by child classes
    constexpr timer_t(uint32_t timeout_us): m_timeout_us(timeout_us) {
        callback = _tmo_key_timer;
    }

    void start_timer(pmap_t* slot);

    void stop_timer();

    bool timer_is_running() const { return xtimer_is_set(this); }

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
    bool timeout_expected();

    // Called by keymap_thread to handle EVENT_TIMEOUT.
    static void handle_timeout(void* arg);

    static void _tmo_key_timer(void* arg);

    const uint32_t m_timeout_us;
    pmap_t* m_slot = nullptr;
    bool m_timeout_expected = false;
};

}  // namespace key
