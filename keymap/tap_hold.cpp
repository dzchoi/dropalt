#define ENABLE_DEBUG 1
#include "debug.h"

#include "tap_hold.hpp"



namespace key {

void tap_hold_t<HOLD_PREFERRED>::on_press(pmap_t* slot)
{
    assert( m_state == NEITHER );
    start_timer(slot);
    start_observe(slot);
}

void tap_hold_t<HOLD_PREFERRED>::on_release(pmap_t* slot)
{
    switch ( m_state ) {
        case NEITHER:
            DEBUG("TapHold: decide tap on release\n");
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

void tap_hold_t<HOLD_PREFERRED>::decide_hold(pmap_t* slot, bool to_hold)
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

void tap_hold_t<HOLD_PREFERRED>::on_timeout(pmap_t* slot)
{
    DEBUG("TapHold: decide hold on timeout\n");
    decide_hold(slot, true);
}

void tap_hold_t<HOLD_PREFERRED>::on_other_press(pmap_t* slot)
{
    DEBUG("TapHold: decide hold on other press\n");
    decide_hold(slot, true);
}

void tap_hold_t<TAP_PREFERRED>::on_other_release(pmap_t* slot)
{
    DEBUG("TapHold: decide tap on other release\n");
    decide_hold(slot, false);
}

void tap_hold_t<BALANCED>::on_other_press(pmap_t* slot)
{
    DEBUG("TapHold: decide hold on other press and release\n");
    decide_hold(slot, true);
}

}  // namespace key
