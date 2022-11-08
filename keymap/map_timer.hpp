#pragma once

#include "xtimer.h"

#include "map.hpp"              // for deriving from map_t
#include "keymap_thread.hpp"    // for signal_timeout()



namespace key {

class map_timer_t: public map_t, public xtimer_t {
public:
    map_timer_t(uint32_t timeout_us): m_timeout_us(timeout_us) {
        callback = _tmo_key_timer;
    }

    map_timer_t* get_timer() { return this; }

    // Will be called when the timer is expired.
    virtual void on_timeout(pmap_t*) =0;

    void start_timer(pmap_t* slot) {
        arg = slot;
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

    ~map_timer_t() { stop_timer(); }

    map_timer_t(const map_timer_t&) =delete;
    void operator=(const map_timer_t&) =delete;

private:
    const uint32_t m_timeout_us;
    bool m_timeout_expected = false;

    static void _tmo_key_timer(void* arg) {
        keymap_thread::obj().signal_timeout(static_cast<pmap_t*>(arg));
    }
};

}  // namespace key
