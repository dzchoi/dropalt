#pragma once

#include "assert.h"             // for assert()
#include "ztimer.h"

#include "event_ext.hpp"        // for event_ext_t



namespace key {

class pmap_t;



class timer_t: public ztimer_t {
protected: // Methods to be used by child classes
    constexpr timer_t(uint32_t timeout_ms)
    : ztimer_t { .base = {}, .callback = _tmo_key_timer, .arg = nullptr }
    , m_timeout_ms(timeout_ms) {}

    // Move constructor is needed for child classes such as tap_hold_t to be movable.
    constexpr timer_t(timer_t&& old)
    : timer_t(old.m_timeout_ms) {
        // This assert() is to disallow the (child) objects being moved at run-time (i.e.
        // while the timer is ticking) and it still will not hurt the `constexpr`, thus
        // enabling the move constructor to run at compile-time and generating the objects
        // in .relocate section rather than in .bss section.
        // Note that `assert()` can be used in constexpr functions as long as the
        // argument is constexpr. See https://stackoverflow.com/questions/32401256.
        assert( m_timeout_expected == nullptr );

        // We could use this assert instead of `assert( m_timeout_expected == nullptr )`
        // above, but it would generate the objects (e.g. `m_1`) in .bss section and
        // initialize them from __static_initialization_and_destruction_0() in
        // __libc_init_array().
        // assert( !old.timer_is_running() );

        // However, we cannot use __builtin_is_constant_evaluated(), because it will also
        // break the `constexpr` like `!old.timer_is_running()` does, and it will return
        // false when run from __static_initialization_and_destruction_0() even though it
        // is before starting the main() function, causing the assert() to always fail.
        // Do not use: assert( __builtin_is_constant_evaluated() );
    }

    // ~timer_t() { assert( m_timeout_expected == nullptr ); }
    // To make it trivial we do not provide a destructor. It is also unnecessary as all
    // map_t (and child) instances are created at compile-time (though mutable) and hence
    // not going to be destroyed. Otherwise it would make the objects generated in .bss
    // section instead of .relocate section.

    void start_timer(pmap_t* slot);

    void stop_timer();

    bool timer_is_running() const { return ztimer_is_set(ZTIMER_MSEC, this); }

    pmap_t* get_slot() const { return m_timeout_expected; }

    timer_t(const timer_t&) =delete;
    timer_t& operator=(const timer_t&) =delete;

private: // Methods to be called by key::manager
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
