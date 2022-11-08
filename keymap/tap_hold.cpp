#define ENABLE_DEBUG 1
#include "debug.h"

#include "tap_hold.hpp"



namespace key {

tap_hold_t::tap_hold_t(map_t& key_tap, map_t& key_hold, uint32_t tapping_term_us)
: map_timer_t(tapping_term_us), m_key_tap(key_tap), m_key_hold(key_hold)
{}

void tap_hold_t::on_press(pmap_t* slot)
{
    assert( !m_holding );
    start_timer(slot);
    start_observe();
    start_defer_presses();
}

void tap_hold_t::on_release(pmap_t* slot)
{
    if ( m_holding ) {
        m_holding = false;
        execute_release(&m_key_hold, slot);
    } else {
        // These can come in any order, except send_release() should follow send_press().
        // DEBUG("TapHold:\e[0;34m decide tap\e[0m\n");
        DEBUG("TapHold: decide tap\n");
        stop_timer();
        stop_observe();
        stop_defer_presses();
        execute_press(&m_key_tap, slot);
        execute_release(&m_key_tap, slot);
    }
}

// Called by on_other_press() and on_timeout().
void tap_hold_t::help_holding(pmap_t* slot)
{
    // DEBUG("TapHold:\e[0;34m decide hold\e[0m\n");
    DEBUG("TapHold: decide hold\n");
    stop_timer();  // will do no harm to stop the already stopped timer.
    stop_observe();
    stop_defer_presses();
    m_holding = true;
    execute_press(&m_key_hold, slot);
}

}  // namespace key
