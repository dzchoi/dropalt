#pragma once

#include "config.hpp"           // for RGB_LED_GCR_MAX



// Control the Global Current Control Register (GCR).
class rgb_gcr {
public:
    // Once set, GCR will gradually ramp toward the desired value over time.
    static void set(uint8_t desired_gcr) { m_desired_gcr = desired_gcr; }

    static void enable() { m_enabled = true; }
    static void disable();
    static bool is_enabled() { return m_enabled; }

    // Invoking isr_process_v_5v_report() from adc::v_5v drives gradual updates to the
    // GCR without having to run a separate timer. Note that SSD is automatically
    // controlled depending on whether GCR is zero or not.
    static void isr_process_v_5v_report();

private:
    constexpr rgb_gcr() =delete;  // Ensure a static class

    static inline bool m_enabled = false;
    static inline uint8_t m_current_gcr = 0;
    static inline uint8_t m_desired_gcr = RGB_LED_GCR_MAX;
};
