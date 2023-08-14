#pragma once

#include "assert.h"             // for assert()
// LOCAL_LOG_LEVEL is controlled by the .cpp that includes this file.
#include "log.h"

#include "defer.hpp"
#include "features.hpp"         // for TAPPING_TERM_MS
#include "map.hpp"
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



// The 'tap-preferred' flavor (or 'default' mode): the output decision is made when
// the tapping_term_ms has expired or the tap-hold key is released. The press or release
// of any other keys within the tapping_term_ms does not affect the decision, but will
// register when the decision is made.
template <class K, class L>
class tap_hold_t<tap_preferred_t, K, L>: public defer_t, public map_t, public timer_t {
public: // User-facing methods
    constexpr tap_hold_t(K&& key_tap, L&& key_hold,
        tap_preferred_t =TAP_PREFERRED, uint32_t tapping_term_ms =TAPPING_TERM_MS)
    : timer_t(tapping_term_ms)
    , m_key_tap(std::forward<K>(key_tap))
    , m_key_hold(std::forward<L>(key_hold)) {}

protected:
    void decide_tap(pmap_t* slot) { help_decide(slot, &m_key_tap); }

    void decide_hold(pmap_t* slot) { help_decide(slot, &m_key_hold); }

private:
    void help_decide(pmap_t* slot, map_t* key_to_choose);

    void on_press(pmap_t* slot);

    void on_release(pmap_t* slot);

    void on_timeout(pmap_t* slot);

    map_t* m_key_chosen = nullptr;  // will be chosen by decide_hold/tap().

    K m_key_tap;
    L m_key_hold;
};

template <class K, class L>
void tap_hold_t<tap_preferred_t, K, L>::help_decide(pmap_t* slot, map_t* key_to_choose)
{
    // The executions of stop_timer(), stop_defer() and .press() can come in any order.
    stop_timer();
    m_key_chosen = key_to_choose;
    m_key_chosen->press(slot);
    stop_defer();
}

template <class K, class L>
void tap_hold_t<tap_preferred_t, K, L>::on_press(pmap_t* slot)
{
    assert( m_key_chosen == nullptr );
    start_timer(slot);
    start_defer(slot);
}

// Todo: This on_release() is called before all deferred events are processed, which will
//  break the original order of the key events. It is ok since we register the press and
//  the release of tapping key instead of holding key. However, if we want to register a
//  holding key (i.e. modifier) we would need to change our keymap driving engine so that
//  the deferrer gets notified twice of its own release, once before the other deferred
//  keys are processed (via on_other_release(nullptr)) and once after they are processed
//  (via on_release(slot)).
template <class K, class L>
void tap_hold_t<tap_preferred_t, K, L>::on_release(pmap_t* slot)
{
    if ( m_key_chosen == nullptr ) {
        LOG_DEBUG("TapHold [%u] decide tap on release\n",
            defer_t::m_slot->index());
        decide_tap(slot);
    }

    m_key_chosen->release(slot);
    m_key_chosen = nullptr;
}

template <class K, class L>
void tap_hold_t<tap_preferred_t, K, L>::on_timeout(pmap_t* slot)
{
    LOG_DEBUG("TapHold [%u] decide hold on timeout\n", slot->index());
    decide_hold(slot);
}



// The 'hold-preferred' flavor (or 'hold on other key press' mode): the hold behavior is
// triggered when tapping_term_ms has expired or another key is pressed within this
// period. If a key that has been pressed previously is released during the period it does
// not affect the decision, but will register when the decision is made.
template <class K, class L>
class tap_hold_t<hold_preferred_t, K, L>: public tap_hold_t<tap_preferred_t, K, L> {
public:
    constexpr tap_hold_t(K&& key_tap, L&& key_hold,
        hold_preferred_t =HOLD_PREFERRED, uint32_t tapping_term_ms =TAPPING_TERM_MS)
    : tap_hold_t<tap_preferred_t, K, L>::tap_hold_t(
        std::forward<K>(key_tap), std::forward<L>(key_hold),
        TAP_PREFERRED, tapping_term_ms)
    {}

private:
    using tap_hold_t<tap_preferred_t, K, L>::decide_hold;

    bool on_other_press(pmap_t* slot);
};

template <class K, class L>
bool tap_hold_t<hold_preferred_t, K, L>::on_other_press(pmap_t* slot)
{
    LOG_DEBUG("TapHold [%u] decide hold on other press\n",
        defer_t::m_slot->index());
    decide_hold(slot);
    return false;
}



// The 'balanced' flavor (or 'permissive hold' mode): the hold behavior is triggered when
// the tapping_term_ms has expired or another key is pressed AND released within this
// period. If a key that has been pressed previously is released during the period it does
// not affect the decision and will register immediately without waiting for the decision.
template <class K, class L>
class tap_hold_t<balanced_t, K, L>: public tap_hold_t<tap_preferred_t, K, L> {
public:
    constexpr tap_hold_t(K&& key_tap, L&& key_hold,
        balanced_t =BALANCED, uint32_t tapping_term_ms =TAPPING_TERM_MS)
    : tap_hold_t<tap_preferred_t, K, L>::tap_hold_t(
        std::forward<K>(key_tap), std::forward<L>(key_hold),
        TAP_PREFERRED, tapping_term_ms)
    {}

private:
    using tap_hold_t<tap_preferred_t, K, L>::decide_hold;

    bool on_other_release(pmap_t* slot);
};

template <class K, class L>
bool tap_hold_t<balanced_t, K, L>::on_other_release(pmap_t* slot)
{
    if ( defer_t::is_deferred(slot, true) ) {
        LOG_DEBUG("TapHold [%u] decide hold on other press and release\n",
            defer_t::m_slot->index());
        decide_hold(slot);
        return false;
    }

    // If it is the release of other key whose press came before our tapping_term, we
    // execute it now and no longer defer it.
    // Todo: In this case, the press of tapping or holding key will get registered after
    //  release of the other key, which will break the original order of the key events.
    //  This is ok mostly, but if the other key happens to be a modifier, the modifier
    //  will end up being released too early.
    return true;
}

}  // namespace key
