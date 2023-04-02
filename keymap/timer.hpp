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
    : ztimer_t { .base = {}, .callback = _tmo_key_timer, .arg = nullptr }
    , m_timeout_ms(old.m_timeout_ms) {
        // Move-constructable only at compile-time.
        constexpr bool is_compile_time = __builtin_is_constant_evaluated();
        // `assert()` can be used in constexpr functions as long as the argument is
        // constexpr. See https://stackoverflow.com/questions/32401256.
        assert( is_compile_time );
    }

    // ~timer_t() { stop_timer(); }
    // We do not provide a destructor and make it trivial. It is also unnecessary as
    // `timer_t` works as a helper base class of map_t child classes, and all map_t
    // instances are declared as a compile-time literal (though mutable) and hence not
    // going to be destroyed at run-time.
    // If a non-trivial destructor were provided or if the constexpr move-constructor
    // above added a non-constexpr expression (hence it breaks being constexpr), any
    // classes derived from timer_t would be allocated in .bss section and thus
    // constructed by __static_initialization_and_destruction_0() at run-time, while they
    // would otherwise be constructed and allocated in .relocate section at compile-time.

    void start_timer(pmap_t* slot);

    void stop_timer();

    bool timer_is_running() const { return ztimer_is_set(ZTIMER_MSEC, this); }

    pmap_t* get_slot() const { return m_timeout_expected; }

    timer_t(const timer_t&) =delete;
    timer_t& operator=(const timer_t&) =delete;

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
