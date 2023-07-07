#pragma once

#include "ztimer.h"             // for ztimer_t, ztimer_set_timeout_flag()

#include <array>                // for std::array<>
#include "color.hpp"            // for hsv_t, ohsv_t
#include "features.hpp"         // for RGB_UPDATE_PERIOD_MS, EFFECT_RIPPLE_MAX_WAVES, ...



class effect_t {
public:
    // Initial update for each led. It is called in the order of led_id (0, 1, ...).
    virtual hsv_t initial_update(uint8_t led_id) =0;

    // Called when all leds are updated initially and refreshed.
    virtual void initial_update_done() {}

    // Update each led at the given time (ms).
    virtual ohsv_t update(uint8_t, uint32_t) { return {}; }

    // Called when all leds are updated and refreshed.
    virtual void update_done() {}

    // Check if the given led is under update.
    virtual bool is_updating(uint8_t) { return false; }

    // Called when a key is pressed/released.
    virtual ohsv_t process_key_event(uint8_t, uint32_t, bool) { return {}; }

protected:
    // Helper method to continue updating leds in period_ms.
    void enable_update_next(ztimer_t& timer, uint32_t period_ms =RGB_UPDATE_PERIOD_MS) {
        ztimer_set_timeout_flag(ZTIMER_MSEC, &timer, period_ms);
    }
};



class fixed_color: public effect_t {
public:
    fixed_color(hsv_t hsv): m_hsv(hsv) {}

    hsv_t initial_update(uint8_t) { return m_hsv; };

private:
    const hsv_t m_hsv;
};



class glimmer: public effect_t {
public:
    glimmer(hsv_t hsv, uint32_t period_ms): m_hsv(hsv), m_period_ms(period_ms) {}

    hsv_t initial_update(uint8_t led_id);

    void initial_update_done();

    ohsv_t update(uint8_t led_id, uint32_t time);

    void update_done();

    bool is_updating(uint8_t) { return true; }

private:
    const hsv_t m_hsv;
    const uint32_t m_period_ms;
    ztimer_t m_timer = {};
    uint8_t m_v;  // Only hsv.v will vary over time.
};



enum state_t: uint8_t {
    DONE = 0,
    RELEASED,
    UPDATING,  // used only for ripple where states are moved down automatically.
    PRESSED
};

// Similar to std::vector<value_t>, but it has a fixed size and does not move elements
// when erasing middle elements. Instead it marks the erased elements as DONE to reuse
// them later. It also traces past to the last non-erased element as customized end().
template <size_t N>
class touched_leds_t {
public:
    struct __attribute__((packed)) value_t {
        uint32_t when_released_ms;
        uint8_t led_id;
        state_t state;
    };

    using array_t = std::array<value_t, N>;
    using iterator = typename array_t::iterator;

    iterator begin() { return m_array.begin(); }
    iterator end() { return m_actual_end; }

    bool empty() const { return m_actual_end == m_array.begin(); }
    iterator find(uint8_t led_id, bool pressed_only);
    iterator create(uint8_t led_id);
    void collect_garbage(bool handle_updating);

private:
    array_t m_array = {};  // initialize with all 0's.
    iterator m_actual_end = m_array.begin();
};

template <size_t N>
typename touched_leds_t<N>::iterator touched_leds_t<N>::create(uint8_t led_id)
{
    iterator it = m_array.begin();
    for ( ; it != m_array.end() ; ++it )
        if ( it->state == DONE ) {
            if ( it == m_actual_end )
                ++m_actual_end;

            // Initialize the members.
            it->when_released_ms = 0;
            it->led_id = led_id;
            it->state = PRESSED;
            break;
        }

    return it;
}

template <size_t N>
typename touched_leds_t<N>::iterator touched_leds_t<N>::find(
    uint8_t led_id, bool pressed_only)
{
    iterator it = begin();
    for ( ; it != end() ; ++it )
        // If pressed_only is true only the (first) element with PRESSED state is found
        // as the tie breaker.
        if ( it->led_id == led_id && (!pressed_only || it->state == PRESSED) )
            break;

    return it;
}

template <size_t N>
void touched_leds_t<N>::collect_garbage(bool handle_updating)
{
    // If handle_updating is true all non-PRESSED states are automatically moved down;
    // RELEASED -> DONE and UPDATING -> RELEASED. So, to not erase elements their states
    // need to keep updating as UPDATING, which is the case of ripple::update().
    if ( handle_updating )
        for ( iterator it = begin() ; it != end() ; ++it ) {
            if ( it->state == RELEASED )
                it->state = DONE;
            else if ( it->state == UPDATING )
                it->state = RELEASED;
        }

    while ( m_actual_end != begin() && (m_actual_end - 1)->state == DONE )
        --m_actual_end;
}



class finger_trace: public effect_t {
public:
    finger_trace(hsv_t hsv, uint32_t restoring_ms);

    hsv_t initial_update(uint8_t led_id);

    ohsv_t update(uint8_t led_id, uint32_t time);

    void update_done();

    // Inherit the base method; even if the led is being updated by Effect, it will not
    // affect indicator lamps and we have to restore it manually on release.
    // bool is_updating(uint8_t) { return false; }

    ohsv_t process_key_event(uint8_t led_id, uint32_t time, bool pressed);

private:
    const hsv_t m_hsv;
    const uint32_t m_restoring_ms;
    ztimer_t m_timer = {};

    touched_leds_t<EFFECT_FINGER_TRACE_MAX_TRACERS> m_touched_leds;
};



class ripple: public effect_t {
public:
    // Here period_ms means the time it takes getting dark and lit again. The wavelength
    // may not be noticeable directly but determines the speed of wave,
    // v = wavelength / period_ms; the larger it is the faster it propagates.
    ripple(hsv_t hsv, uint32_t period_ms, uint32_t wavelength);

    hsv_t initial_update(uint8_t led_id);

    ohsv_t update(uint8_t led_id, uint32_t time);

    void update_done();

    bool is_updating(uint8_t) { return !m_touched_leds.empty(); }

    ohsv_t process_key_event(uint8_t led_id, uint32_t time, bool pressed);

private:
    const hsv_t m_hsv;
    const uint32_t m_period_ms;
    const uint32_t m_wavelength;
    ztimer_t m_timer = {};

    touched_leds_t<EFFECT_RIPPLE_MAX_WAVES> m_touched_leds;
};
