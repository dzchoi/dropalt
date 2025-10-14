#pragma once

#include "mutex.h"              // for mutex_t
#include "thread.h"             // for thread_t

#include "config.hpp"           // for DEBOUNCE_PRESS_MS, DEBOUNCE_RELEASE_MS



class matrix_thread {
public:
    static void init();

    static bool is_idle() { return !m_is_polling; }

private:
    constexpr matrix_thread() =delete;  // Ensure a static class.

    static thread_t* m_pthread;

    static char m_thread_stack[];

    union bounce_state_t {
        uint8_t uint8;
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

    static mutex_t m_sleep_lock;

    static uint32_t m_wakeup_us;

    static int m_min_scan_count;

    static bool m_is_polling;

    // thread body
    static void* _thread_entry(void* arg);

    // Uses asymmetric variation of the integration debouncing algorithm.
    // (See https://www.kennethkuhn.com/electronics/debounce.c)
    //  - Asymmetric: detects press and release events with different detection delays.
    //  - Per-key: maintains a separate debouncer for each key.
    //  - Scan mode: uses active (polling) scan while any key is pressed; switches to
    //    interrupt-based scanning once all keys are released.
    static void _debouncer(unsigned mat_index, bool pressing);

    static void _isr_any_key_down(void* arg);
};
