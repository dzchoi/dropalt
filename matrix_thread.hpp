#pragma once

#include "matrix.h"
#include "thread.h"

#include <algorithm>            // for std::max()
#include <utility>              // for std::pair
#include "features.hpp"         // for DEBOUNCE_TIME_US
#include "xtimer_wrapper.hpp"



class matrix_thread {
public:
    static matrix_thread& obj() {
        static matrix_thread obj;
        return obj;
    }

private:
    thread_t* const m_pthread;

    // THREAD_STACKSIZE_TINY cannot be used with printf().
    char m_stack[THREAD_STACKSIZE_SMALL];

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

    // helper method to copy raw_matrix[] to matrix[].
    std::pair<bool, bool> matrix_copy();

    void start_scan();
    void continue_scan();
    void debounce_done();

    static void _isr_detect_any_key_down(void* arg);

    xtimer_periodic_signal_t scan_timer;
    xtimer_onetime_signal_t debounce_timer;

    enum {
        FLAG_EVENT              = 0x0001,  // == THREAD_FLAG_EVENT from event.h
        FLAG_START_SCAN         = 0x0002,
        FLAG_DEBOUNCE_TIMEOUT   = 0x0004,
        FLAG_SCAN_TIMEOUT       = THREAD_FLAG_TIMEOUT  // (1u << 14)
    };
};
