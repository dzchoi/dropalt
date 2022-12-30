#pragma once

#include "thread.h"             // for thread_t
#include "thread_flags.h"       // for THREAD_FLAG_TIMEOUT
#include "ztimer.h"             // for ztimer_t



class matrix_thread {
public:
    static matrix_thread& obj() {
        static matrix_thread obj;
        return obj;
    }

private:
    thread_t* m_pthread;

#ifdef DEVELHELP
    char m_stack[THREAD_STACKSIZE_MEDIUM];
#else
    char m_stack[THREAD_STACKSIZE_SMALL];
#endif

    matrix_thread();  // Can be called only by obj().

    // thread body
    static void* _matrix_thread(void* arg);

    // Asymmetric, Interrupt/Timer-based, and Per-key debouncing algorithm
    //  - Asymmetric: eager presses but deferred releases (Deferred - wait for debounce
    //    time before reporting change)
    //  - Timer-based scan while any key is being pressed, then interrupt-based when all
    //    keys are released.
    //  - Per-key: one timer per key (when releasing).

    // Implement the eager debouncing algorithm, returning the decision of whether the
    // key at (row, col) is being pressed.
    static bool _debouncer(unsigned row, unsigned col, bool is_pressed);

    void perform_scan();

    static void _isr_detect_any_key_down(void* arg);

    ztimer_t m_scan_timer = {};

    bool m_first_scan;  // no need to initialize.

    enum {
        FLAG_EVENT              = 0x0001,  // not used
        FLAG_SCAN               = THREAD_FLAG_TIMEOUT  // (1u << 14)
    };
};
