#pragma once

#include "periph_conf.h"        // for NUM_SLOTS
#include "thread.h"             // for thread_t
#include "thread_flags.h"       // for THREAD_FLAG_TIMEOUT
#include "ztimer.h"             // for ztimer_t, ztimer_now()



class matrix_thread {
public:
    static matrix_thread& obj() {
        static matrix_thread obj;
        return obj;
    }

    bool is_pressed(size_t index) const { return m_states[index].is_press; }

    bool is_any_pressed() const { return m_any_pressed; }

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

    // Stateshift debouncing algorithm
    // (See https://sev.dev/hardware/better-debounce-algorithms/)
    //  - asymmetric: determines press quicker than release.
    //  - per-key: separate debouncer per each key.
    //  - periodic scan while any key is being pressed, whereas interrupt-based scan
    //    after all keys are released.
    union {
        int8_t data;
        struct {
            uint8_t history: 7;
            uint8_t is_press: 1;  // sign bit of `data` (i.e. true iff data < 0)
        };
    } m_states[NUM_SLOTS];
    static_assert( sizeof(m_states[0]) == 1 );

    static void _debouncer(unsigned row, unsigned col, bool is_press);
    static inline bool state_needs_changing(int8_t state);

    void perform_scan();

    // Typical time for executing matrix_scan() and walking through the result. It will be
    // compensated from MATRIX_SCAN_PERIOD_US.
    static constexpr uint32_t TYPICAL_SCAN_TIME_US = 125;

    static void _isr_detect_any_key_down(void* arg);

    ztimer_t m_scan_timer = {};

    unsigned m_first_scan = 0;

    bool m_any_pressed = false;

    enum {
        FLAG_EVENT              = 0x0001,  // not used
        FLAG_SCAN               = THREAD_FLAG_TIMEOUT  // (1u << 14)
    };
};
