#define ENABLE_DEBUG 1
#include "debug.h"

#include "tap_hold.hpp"



namespace key {

void tap_hold_t<TAP_PREFERRED>::on_press(pmap_t* slot)
{
    assert( !m_holding );
    start_timer(slot);
    start_observe();
}

void tap_hold_t<TAP_PREFERRED>::on_release(pmap_t* slot)
{
    if ( m_holding ) {
        m_holding = false;
        m_key_hold.release(slot);
    } else {
        // These can come in any order, except send_release() should follow send_press().
        // DEBUG("TapHold:\e[0;34m decide tap\e[0m\n");
        DEBUG("TapHold: decide tap\n");
        stop_timer();
        stop_observe();
        m_key_tap.press(slot);
        m_key_tap.release(slot);
    }
}

void tap_hold_t<TAP_PREFERRED>::on_timeout(pmap_t* slot)
{
    // DEBUG("TapHold:\e[0;34m decide hold\e[0m\n");
    DEBUG("TapHold: decide hold\n");
    stop_observe();
    m_holding = true;
    m_key_hold.press(slot);
}

void tap_hold_t<TAP_PREFERRED>::on_other_press(pmap_t* slot)
{
    stop_timer();
    on_timeout(slot);
}

}  // namespace key
