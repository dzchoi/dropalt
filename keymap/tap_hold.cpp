#define ENABLE_DEBUG 1
#include "debug.h"

#include "literal.hpp"          // for keycode()
#include "tap_hold.hpp"



namespace key {

tap_hold_t::tap_hold_t(const literal_t& key_tap, const literal_t& key_hold,
    uint32_t tapping_term_us)
: timer_t(tapping_term_us)
, m_code_tap(key_tap.keycode()), m_code_hold(key_hold.keycode())
{}

void tap_hold_t::on_press(pbase_t* ppbase)
{
    start_timer(ppbase);
    start_observe();
    start_defer_presses();
    if ( m_holding )
        DEBUG("TapHold:\e[1;31m spurious holding (0x%x)\e[0m\n", m_code_tap);
}

void tap_hold_t::on_release(pbase_t*)
{
    if ( m_holding ) {
        m_holding = false;
        send_release(m_code_hold);
    } else {
        // These can come in any order, except send_release() should follow send_press().
        DEBUG("TapHold:\e[1;34m decide tap\e[0m\n");
        stop_timer();
        stop_observe();
        stop_defer_presses();
        send_press(m_code_tap);
        send_release(m_code_tap);
    }
}

// Called by on_other_press() and on_timeout().
void tap_hold_t::help_holding()
{
    DEBUG("TapHold:\e[1;34m decide hold\e[0m\n");
    stop_timer();  // will do no harm to stop the already stopped timer.
    stop_observe();
    stop_defer_presses();
    m_holding = true;
    send_press(m_code_hold);
}

}  // namespace key
