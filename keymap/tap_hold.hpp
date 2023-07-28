#pragma once

// LOCAL_LOG_LEVEL is controlled by the .cpp that includes this file.
#include "log.h"

#include "features.hpp"         // for TAPPING_TERM_MS
#include "map.hpp"
#include "observer.hpp"
#include "timer.hpp"



namespace key {

// Flavors of tap_hold_t
constexpr struct tap_preferred_t {} TAP_PREFERRED;  // default
constexpr struct hold_preferred_t {} HOLD_PREFERRED;
constexpr struct balanced_t {} BALANCED;

template <class, class, class>
class tap_hold_t;

// Deduction guides to be able to use TAP_PREFERRED by default.
template <class K, class L>
tap_hold_t(K&&, L&&) ->
    tap_hold_t<tap_preferred_t, obj_or_ref_t<K>, obj_or_ref_t<L>>;

template <class F, class K, class L>
tap_hold_t(K&&, L&&, F) ->
    tap_hold_t<F, obj_or_ref_t<K>, obj_or_ref_t<L>>;

template <class F, class K, class L>
tap_hold_t(K&&, L&&, F, uint32_t) ->
    tap_hold_t<F, obj_or_ref_t<K>, obj_or_ref_t<L>>;



// The 'hold-preferred' flavor (or 'hold on other key press' mode): the hold behavior is
// triggered when tapping_term_ms has expired or another key is pressed within this
// period.
template <class K, class L>
class tap_hold_t<hold_preferred_t, K, L>: public map_t, public timer_t, public observer_t {
public: // User-facing methods
    constexpr tap_hold_t(K&& key_tap, L&& key_hold,
        hold_preferred_t =HOLD_PREFERRED, uint32_t tapping_term_ms =TAPPING_TERM_MS)
    : timer_t(tapping_term_ms)
    , m_key_tap(std::forward<K>(key_tap))
    , m_key_hold(std::forward<L>(key_hold)) {}

protected:
    void decide_hold(pmap_t* slot, bool to_hold);

private: // Methods to be called by key::manager
    void on_press(pmap_t* slot);

    void on_release(pmap_t* slot);

    void on_timeout(pmap_t* slot);

    void on_other_press(pmap_t* slot);

    virtual void start_observe(pmap_t* slot) { observer_t::start_observe(slot); }

    virtual void stop_observe() { observer_t::stop_observe(); }

    K m_key_tap;
    L m_key_hold;

    enum {
        NEITHER = 0,
        TAPPING,
        HOLDING,
    } m_state = NEITHER;
};

template <class K, class L>
void tap_hold_t<hold_preferred_t, K, L>::decide_hold(pmap_t* slot, bool to_hold)
{
    // The executions of stop_timer(), stop_observe() and .press() can come in any order.
    stop_timer();
    stop_observe();
    if ( to_hold ) {
        m_state = HOLDING;
        m_key_hold.press(slot);
    }
    else {
        m_state = TAPPING;
        m_key_tap.press(slot);
    }
}

template <class K, class L>
void tap_hold_t<hold_preferred_t, K, L>::on_press(pmap_t* slot)
{
    assert( m_state == NEITHER );
    start_timer(slot);
    start_observe(slot);
}

template <class K, class L>
void tap_hold_t<hold_preferred_t, K, L>::on_release(pmap_t* slot)
{
    switch ( m_state ) {
        case NEITHER:
            LOG_DEBUG("TapHold: decide tap on release\n");
            decide_hold(slot, false);
            // Intentional fall-through

        case TAPPING:
            m_key_tap.release(slot);
            break;

        case HOLDING:
            m_key_hold.release(slot);
            break;
    }

    m_state = NEITHER;
}

template <class K, class L>
void tap_hold_t<hold_preferred_t, K, L>::on_timeout(pmap_t* slot)
{
    LOG_DEBUG("TapHold: decide hold on timeout\n");
    decide_hold(slot, true);
}

template <class K, class L>
void tap_hold_t<hold_preferred_t, K, L>::on_other_press(pmap_t* slot)
{
    LOG_DEBUG("TapHold: decide hold on other press\n");
    decide_hold(slot, true);
}



// The 'tap-preferred' flavor (or 'default' mode): the hold behavior is triggered when
// the tapping_term_ms has expired. If a key is pressed or released (including the
// tapping key itself being released) within this period the tapping behavior is
// triggered.
template <class K, class L>
class tap_hold_t<tap_preferred_t, K, L>: public tap_hold_t<hold_preferred_t, K, L> {
public:
    constexpr tap_hold_t(K&& key_tap, L&& key_hold,
        tap_preferred_t =TAP_PREFERRED, uint32_t tapping_term_ms =TAPPING_TERM_MS)
    : tap_hold_t<hold_preferred_t, K, L>::tap_hold_t(
        std::forward<K>(key_tap), std::forward<L>(key_hold),
        HOLD_PREFERRED, tapping_term_ms)
    {}

private:
    using tap_hold_t<hold_preferred_t, K, L>::decide_hold;

    void on_other_press(pmap_t* slot);

    void on_other_release(pmap_t* slot);
};

template <class K, class L>
void tap_hold_t<tap_preferred_t, K, L>::on_other_press(pmap_t* slot)
{
    LOG_DEBUG("TapHold: decide tap on other press\n");
    decide_hold(slot, false);
}

template <class K, class L>
void tap_hold_t<tap_preferred_t, K, L>::on_other_release(pmap_t* slot)
{
    LOG_DEBUG("TapHold: decide tap on other release\n");
    decide_hold(slot, false);
}



// The 'balanced' flavor (or 'permissive hold' mode): the hold behavior is triggered when
// the tapping_term_ms has expired or another key is pressed AND released within this
// period. If a key that has been pressed previously is released during the period it does
// not affect the decision.
template <class K, class L>
class tap_hold_t<balanced_t, K, L>: public tap_hold_t<hold_preferred_t, K, L> {
public:
    constexpr tap_hold_t(K&& key_tap, L&& key_hold,
        balanced_t =BALANCED, uint32_t tapping_term_ms =TAPPING_TERM_MS)
    : tap_hold_t<hold_preferred_t, K, L>::tap_hold_t(
        std::forward<K>(key_tap), std::forward<L>(key_hold),
        HOLD_PREFERRED, tapping_term_ms)
    {}

private:
    using tap_hold_t<hold_preferred_t, K, L>::decide_hold;

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

template <class K, class L>
void tap_hold_t<balanced_t, K, L>::on_other_press(pmap_t* slot)
{
    LOG_DEBUG("TapHold: decide hold on other press and release\n");
    decide_hold(slot, true);
}

}  // namespace key
