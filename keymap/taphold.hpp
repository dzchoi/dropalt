#pragma once

#define ENABLE_DEBUG 1
#include "debug.h"

#include "basic.hpp"            // for keycode()
#include "observer.hpp"
#include "timer.hpp"



namespace key {

// tap_hold_t works on top of two basic_t keys.
class tap_hold_t: public map_t, public observer_t, public timer_t {
public:
    tap_hold_t(const basic_t& key_tap, const basic_t& key_hold,
        uint32_t tapping_term_us =TAPPING_TERM_US)
    : timer_t(tapping_term_us)
    , m_code_tap(key_tap.keycode()), m_code_hold(key_hold.keycode()) {}

    timer_t* get_timer() { return dynamic_cast<timer_t*>(this); }

    void on_press(pmap_t* ppmap) {
        start_timer(ppmap);
        start_observe();
        if ( m_holding )
            DEBUG("TapHold:\e[1;31m spurious holding (0x%x)\e[0m\n", m_code_tap);
    }

    void on_release(pmap_t*) {
        if ( m_holding ) {
            m_holding = false;
            send_release(m_code_hold);
        } else {
            stop_timer();
            stop_observe();
            send_press(m_code_tap);
            send_release(m_code_tap);
        }
    }

    void on_other_press(pmap_t*) {
        DEBUG("---\e[0;34m Hold preferred\e[0m\n");
        stop_timer();
        stop_observe();
        m_holding = true;
        send_press(m_code_hold);
    }

    void on_timeout(pmap_t*) {
        stop_observe();
        m_holding = true;
        send_press(m_code_hold);
    }

    bool is_pressing() const { return m_holding; }

private:
    const uint8_t m_code_tap;
    const uint8_t m_code_hold;

    bool m_holding = false;
};

}  // namespace key
