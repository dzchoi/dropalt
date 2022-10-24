#pragma once

#include "keymap.hpp"
#include "observer.hpp"
#include "timer.hpp"



namespace key {

// The 'hold-preferred' flavor: the hold behavior is triggered when tapping_term_us has
// expired or another key is pressed within this period.
class tap_hold_t: public base_t, public observer_t, public timer_t {
public:
    tap_hold_t(const literal_t& key_tap, const literal_t& key_hold,
        uint32_t tapping_term_us =TAPPING_TERM_US);

    timer_t* get_timer() { return dynamic_cast<timer_t*>(this); }

    void on_press(pbase_t*);

    void on_release(pbase_t*);

    void on_other_press(pbase_t*) { help_holding(); }

    void on_timeout(pbase_t*) { help_holding(); }

    bool is_pressing() const { return m_holding; }

private:
    const uint8_t m_code_tap;
    const uint8_t m_code_hold;

    bool m_holding = false;

    void help_holding();

    virtual void start_defer_presses() {}
    virtual void stop_defer_presses() {}
};



// The 'balanced' flavor (or "permissive hold" mode): the hold behavior is triggered when
// the tapping_term_us has expired or another key is pressed and RELEASED within this
// period. The decision to either tap or hold is made actually slower than tap_hold_t but
// is more suitable for fast typists in general.
class tap_hold_fast_t: public tap_hold_t {
public:
    using tap_hold_t::tap_hold_t;

private:
    void start_defer_presses() { base_t::start_defer_presses(); }
    void stop_defer_presses() { base_t::stop_defer_presses(); }
};

}  // namespace key
