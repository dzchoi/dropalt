#pragma once

#include "map.hpp"
#include "observer.hpp"
#include "timer.hpp"



namespace key {

// The 'hold-preferred' flavor: the hold behavior is triggered when tapping_term_us has
// expired or another key is pressed within this period.
class tap_hold_t: public map_t, public timer_t, public observer_t {
public: // User-facing methods
    constexpr tap_hold_t(map_t& key_tap, map_t& key_hold,
        uint32_t tapping_term_us =TAPPING_TERM_US)
    : timer_t(tapping_term_us), m_key_tap(key_tap), m_key_hold(key_hold) {}

private: // Methods to be called by key::manager
    void on_press(pmap_t* slot);

    void on_release(pmap_t* slot);

    void on_other_press(pmap_t* slot) { help_holding(slot); }

    void on_timeout(pmap_t* slot) { help_holding(slot); }

    void help_holding(pmap_t* slot);

    virtual void start_defer_presses() {}
    virtual void stop_defer_presses() {}

    map_t& m_key_tap;
    map_t& m_key_hold;

    bool m_holding = false;
};



// The 'balanced' flavor (or "permissive hold" mode): the hold behavior is triggered when
// the tapping_term_us has expired or another key is pressed and RELEASED within this
// period. The decision to either tap or hold is made actually slower than tap_hold_t but
// is more suitable for fast typists in general.
class tap_hold_fast_t: public tap_hold_t {
public:
    using tap_hold_t::tap_hold_t;

private:
    void start_defer_presses() { map_t::start_defer_presses(); }
    void stop_defer_presses() { map_t::stop_defer_presses(); }
};

}  // namespace key
