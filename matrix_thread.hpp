#pragma once

#include "matrix.h"
#include "thread.h"
#include "xtimer.h"

#include "features.hpp"         // for DEBOUNCE_TIME_US



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

    // matrix state (1:on, 0:off)
    matrix_row_t raw_matrix[MATRIX_ROWS];  // raw values
    matrix_row_t matrix[MATRIX_ROWS];      // cooked values after debounce

    // thread body
    static void* _matrix_thread(void* arg);

    // Symmetric, deferred and global debouncing algorithm
    //  - Symmetric: same debounce time for both key-press and release
    //  - Deferred: wait for debounce time before reporting change
    //  - Global: one timer for all keys. Any key change state affects the global timer.
    //  - Timer-based scan while any key pressing, then goes interrupt-based when idle.

    void detect_change(bool first_scan);
    bool commit_change();

    static void _isr_detect_any_key_down(void* arg);

    xtimer_t scan_timer;
    uint32_t debounce_started;  // no need to initialize.

    bool is_debounce_done() const {
        return debounce_started
            && int32_t(xtimer_now_usec() - debounce_started) >= int32_t(DEBOUNCE_TIME_US);
    }

    enum {
        FLAG_EVENT              = 0x0001,  // == THREAD_FLAG_EVENT from event.h
        FLAG_START_SCAN         = 0x0002,
        FLAG_CONTINUE_SCAN      = THREAD_FLAG_TIMEOUT  // (1u << 14)
    };
};
