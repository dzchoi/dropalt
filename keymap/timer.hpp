#pragma once

#include "xtimer.h"

#include "keymap_thread.hpp"    // for signal_timeout()



namespace key {

class timer_t: xtimer_t {
public:
    timer_t(uint32_t timeout_us): m_timeout_us(timeout_us) {
        callback = _tmo_key_timer;
    }

    // Will be called when the timer is expired.
    virtual void on_timeout(pmap_t*) =0;

    void start_timer(pmap_t* ppmap) {
        arg = ppmap;
        xtimer_set(this, m_timeout_us);
        m_timeout_expected = true;
    }

    void stop_timer() {
        xtimer_remove(this);
        m_timeout_expected = false;
    }

    // Inherent problem with stopping a timer is that it is not an atomic operation and
    // we can still have a "spurious" timeout interrupt before/during the operation. To
    // deal with the problem, after a timeout interrupt occurred, we can check if it is
    // legitimate and expected timeout, discarding it if not.
    bool timeout_expected() {
        const bool timeout_expected = m_timeout_expected;
        m_timeout_expected = false;
        return timeout_expected;
    }

    bool timer_is_running() const { return xtimer_is_set(this); }

    ~timer_t() { stop_timer(); }

    timer_t(const timer_t&) =delete;
    void operator=(const timer_t&) =delete;

private:
    const uint32_t m_timeout_us;
    bool m_timeout_expected = false;

    static void _tmo_key_timer(void* arg) {
        keymap_thread::obj().signal_timeout(static_cast<pmap_t*>(arg));
    }
};

}  // namespace key
