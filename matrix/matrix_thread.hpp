#pragma once

#include "thread.h"             // for thread_t

#include "config.hpp"           // for DEBOUNCE_PRESS_MS, DEBOUNCE_RELEASE_MS



class matrix_thread {
public:
    static void init();

    static bool is_idle() { return thread_get_status(m_pthread) == STATUS_SLEEPING; }

    // Put the thread in STATUS_SLEEPING. It will be effective after transitioning to
    // interrupt-based scanning.
    static void disable() { m_enabled = false; }

    static void enable() { m_enabled = true; }

private:
    constexpr matrix_thread() =delete;  // Ensure a static class.

    static thread_t* m_pthread;

    static bool m_enabled;

    static char m_thread_stack[];

    static_assert( DEBOUNCE_PRESS_MS >= 1 );
    static_assert( DEBOUNCE_RELEASE_MS >= 1 );

    // Per-key debounce state (magnitude = counter, sign = pressing/not).
    static int8_t m_bounce[];

    // Press/release state reported to main_thread. Kept in its own byte so the hot
    // debouncer only touches m_bounce.
    static bool m_pressed[];

    static uint32_t m_wakeup_us;

    static int m_min_scan_count;

    // thread body
    static void* _thread_entry(void* arg);

    static void _isr_any_key_down(void* arg);
};
