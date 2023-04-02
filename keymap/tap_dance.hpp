#pragma once

#include "features.hpp"         // for TAPPING_TERM_MS
#include "map_dance.hpp"



namespace key {

// Send the first key when tapped once, the second key otherwise. When the key is held,
// the appropriate key is registered: the first one when pressed and held, the second
// one when tapped once, then pressed and held.
template <class K, class L>
class tap_dance_double_t: public map_dance_t {
public:
    constexpr tap_dance_double_t(
        K&& once, L&& twice, uint32_t tapping_term_ms =TAPPING_TERM_MS)
    : map_dance_t(tapping_term_ms)
    , m_once(std::forward<K>(once)), m_twice(std::forward<L>(twice)) {}

private:
    void on_press(pmap_t* slot) {
        if ( get_step() == 1 )
            m_once.press(slot);
        else {
            m_once.release(slot);
            m_twice.press(slot);
            finish();
        }
    }

    void on_release(pmap_t* slot) {
        if ( get_step() == 1 )
            m_once.release(slot);
        else
            m_twice.release(slot);
    }

    K m_once;
    L m_twice;
};

template <class K, class L>
tap_dance_double_t(K&&, L&&) ->
    tap_dance_double_t<obj_or_ref_t<K>, obj_or_ref_t<L>>;

template <class K, class L>
tap_dance_double_t(K&&, L&&, uint32_t) ->
    tap_dance_double_t<obj_or_ref_t<K>, obj_or_ref_t<L>>;

}  // namespace key
