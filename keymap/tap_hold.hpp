#pragma once

#include "features.hpp"         // for TAPPING_TERM_MS
#include "map.hpp"
#include "observer.hpp"
#include "timer.hpp"



namespace key {

enum tap_hold_flavor {
    TAP_PREFERRED = 0,
    HOLD_PREFERRED,
    BALANCED,
};

template <tap_hold_flavor>
class tap_hold_t;

// Deduction guides to be able to use bare tap_hold_t as tap_hold_t<TAP_PREFERRED>
tap_hold_t(map_t&, map_t&) -> tap_hold_t<TAP_PREFERRED>;
tap_hold_t(map_t&, map_t&, uint32_t) -> tap_hold_t<TAP_PREFERRED>;



// The 'hold-preferred' flavor (or 'hold on other key press' mode): the hold behavior is
// triggered when tapping_term_ms has expired or another key is pressed within this
// period.
template <>
class tap_hold_t<HOLD_PREFERRED>: public map_t, public timer_t, public observer_t {
public: // User-facing methods
    constexpr tap_hold_t(map_t& key_tap, map_t& key_hold,
        uint32_t tapping_term_ms =TAPPING_TERM_MS)
    : timer_t(tapping_term_ms), m_key_tap(key_tap), m_key_hold(key_hold) {}

    // Todo: Accept rvalue reference for convenience.
    //  template <class T>
    //  tap_hold_t(T&& key_tap, ...)
    //  : m_key_tap(create(std::move(key_tap))), ... {}
    //  where map_t& create(T&& key) { return *new T(std::move(key)); }
    //  However, then we cannot creat it at compile time (`new` will refuse to be in
    //  constexpr constructor) and we will also need to be able to move timer_t.

protected:
    void decide_hold(pmap_t* slot, bool to_hold);

private: // Methods to be called by key::manager
    void on_press(pmap_t* slot);

    void on_release(pmap_t* slot);

    void on_timeout(pmap_t* slot);

    void on_other_press(pmap_t* slot);

    virtual void start_observe(pmap_t* slot) { observer_t::start_observe(slot); }

    virtual void stop_observe() { observer_t::stop_observe(); }

    map_t& m_key_tap;
    map_t& m_key_hold;

    enum {
        NEITHER = 0,
        TAPPING,
        HOLDING,
    } m_state = NEITHER;
};



// The 'tap-preferred' flavor (or 'default' mode): the hold behavior is triggered when
// the tapping_term_ms has expired. If a key (including the tapping key itself) is
// released within this period the tapping behavior is triggered. Pressing another key
// during the period does not affect the decision.
template <>
class tap_hold_t<TAP_PREFERRED>: public tap_hold_t<HOLD_PREFERRED> {
public:
    using tap_hold_t<HOLD_PREFERRED>::tap_hold_t;

private:
    void on_other_press(pmap_t*) {}

    void on_other_release(pmap_t* slot);
};



// The 'balanced' flavor (or 'permissive hold' mode): the hold behavior is triggered when
// the tapping_term_ms has expired or another key is pressed AND released within this
// period. If a key that has been pressed previously is released during the period it does
// not affect the decision.
template <>
class tap_hold_t<BALANCED>: public tap_hold_t<HOLD_PREFERRED> {
public:
    using tap_hold_t<HOLD_PREFERRED>::tap_hold_t;

private:
    void on_other_press(pmap_t* slot);

    void start_observe(pmap_t* slot) {
        observer_t::start_observe(slot);
        map_t::start_defer_presses();
    }

    void stop_observe() {
        observer_t::stop_observe();
        map_t::stop_defer_presses();
    }
};

}  // namespace key
