#pragma once

#include "features.hpp"         // for TAPPING_TERM_MS
#include "map_dance.hpp"



namespace key {

// Send the first key when tapped once, the second key otherwise. When the key is held,
// the appropriate key is registered: the first one when pressed and held, the second
// one when tapped once, then pressed and held.
class tap_dance_double_t: public map_dance_t {
public:
    constexpr tap_dance_double_t(
        map_t& once, map_t& twice, uint32_t tapping_term_ms =TAPPING_TERM_MS)
    : map_dance_t(tapping_term_ms), m_once(once), m_twice(twice) {}

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

    map_t& m_once;
    map_t& m_twice;
};

}  // namespace key
