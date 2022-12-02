#pragma once

#include "ztimer.h"             // for ztimer_t, ztimer_set_timeout_flag()

#include <optional>             // for std::optional<>
#include <vector>               // for std::vector<>
#include "color.hpp"            // for hsv_t
#include "features.hpp"         // for RGB_UPDATE_PERIOD_MS



using ohsv_t = std::optional<hsv_t>;



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

private:
    const hsv_t m_hsv;
    const uint32_t m_period_ms;
    ztimer_t m_timer;
    uint8_t m_v;  // Only hsv.v will vary over time.
};



class finger_trace: public effect_t {
public:
    finger_trace(hsv_t hsv, uint32_t restoring_ms);

    hsv_t initial_update(uint8_t led_id);

    ohsv_t update(uint8_t led_id, uint32_t time);

    void update_done();

    ohsv_t process_key_event(uint8_t led_id, uint32_t time, bool pressed);

private:
    const hsv_t m_hsv;
    const uint32_t m_restoring_ms;
    ztimer_t m_timer;

    struct __attribute__((packed)) touched_t {
        uint32_t when_released_ms;
        uint8_t led_id;
        enum : uint8_t { FINISHED = 0, RELEASED, PRESSED } state;
    };
    std::vector<touched_t> m_touched_leds;
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

    ohsv_t process_key_event(uint8_t led_id, uint32_t time, bool pressed);

private:
    const hsv_t m_hsv;
    const uint32_t m_period_ms;
    const uint32_t m_wavelength;
    ztimer_t m_timer;

    struct __attribute__((packed)) touched_t {
        uint32_t when_released_ms;
        uint8_t led_id;
        enum : uint8_t { FINISHED = 0, RELEASED, PRESSED } state;
    };
    std::vector<touched_t> m_touched_leds;
};
