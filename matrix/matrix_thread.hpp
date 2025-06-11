#pragma once

#include "thread.h"             // for thread_t, thread_get_status()

#include "config.hpp"           // for DEBOUNCE_PRESS_MS, DEBOUNCE_RELEASE_MS



class matrix_thread {
public:
    static void init();

    static bool is_pressed(unsigned slot_index) { return m_states[slot_index].pressed; }

    static bool is_any_pressed() {
        // Note that the thread stays in STATUS_MUTEX_BLOCKED during
        // ztimer_periodic_wakeup().
        return thread_get_status(m_pthread) > STATUS_SLEEPING;
    }

private:
    constexpr matrix_thread() =delete;  // Ensure a static class.

    static thread_t* m_pthread;

    static char m_thread_stack[];

    union bounce_state_t {
        uint8_t value;
        struct {
            uint8_t counter: 6;
            bool pressed: 1;
            bool pressing: 1;
        };
    };
    static_assert( sizeof(bounce_state_t) == sizeof(uint8_t) );
    static_assert( DEBOUNCE_PRESS_MS < (1u << 6) );
    static_assert( DEBOUNCE_RELEASE_MS < (1u << 6) );

    static bounce_state_t m_states[];

    static uint32_t m_wakeup_us;

    static unsigned m_first_scan;

    // thread body
    static void* _thread_entry(void* arg);

    // Uses asymmetric variation of the integration debouncing algorithm.
    // (See https://www.kennethkuhn.com/electronics/debounce.c)
    //  - Asymmetric: detect press faster than release or vice versa.
    //  - Per-key: separate debouncer per each key.
    //  - Periodic scan while any key is being pressed, whereas interrupt-based scan
    //    after all keys are released.
    static void _debouncer(unsigned slot_index, bool pressing);

    static void _isr_any_key_down(void* arg);
};
