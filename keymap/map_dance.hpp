#pragma once

#include "defer.hpp"
#include "map_proxy.hpp"
#include "timer.hpp"



namespace key {

// Similar to QMK's ACTION_TAP_DANCE_FN_ADVANCED(), only function names differ.
//  - on_press() is called every time you press the tap dance key.
//  - on_finish() gets called when the tap dance finishes (either when tapping_term_ms
//    has elapsed since the tap dance key was PRESSED last time, or when any other key is
//    pressed within the tapping_term_ms.) Then on_release() is called immediately after.
//  - However, the tap dance can also be finished by explicitly calling finish() (from
//    on_press()). In this case on_finish() will be skipped to call and on_release() will
//    be called when the key is released later.
//  - So on_release() is called when the tap dance key is released if the tap dance was
//    finished explicitly, or when the tap dance finishes after the key is released.
//  - The call sequence is on_press(), on_press(), ..., [on_finish()], on_release().
//  - The tap count can be referenced in on_press/finish/release() using .get_step().
class map_dance_t: public defer_t, public map_proxy_t, public timer_t {
protected: // Methods to be used by child classes
    constexpr map_dance_t(uint32_t tapping_term_ms): timer_t(tapping_term_ms) {}

    void finish();

    uint8_t get_step() const { return m_step; }

private: // Methods to be called by key::manager
    void on_proxy_press(pmap_t* slot);

    void on_proxy_release(pmap_t* slot);

    void on_timeout(pmap_t* slot);

    bool on_other_press(pmap_t* slot);

    // Todo: Do we need a parameter to on_finish that will indicate whether on_finish()
    //  is called from on_timeout() or from on_other_press() (i.e. interrupted by other
    //  key)?
    virtual void on_finish(pmap_t*) {};

    // m_step gets increased before every calling of on_press() and resets to 0 after
    // calling on_release().
    uint8_t m_step = 0;

    // We could also use m_step = 0 to indicate that the dance is finished, but we have
    // a separate indicator, which will enable on_release() to use m_step inside.
    bool m_is_finished = true;
};

}  // namespace key
