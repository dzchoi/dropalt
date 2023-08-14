#include "board.h"              // for THREAD_PRIO_MATRIX
#include "log.h"
#include "matrix.h"             // for matrix_init(), matrix_scan(), ...
#include "thread_flags.h"       // for thread_flags_wait_any(), thread_flags_set()
#include "ztimer.h"             // for ztimer_set_timeout_flag()

#include "features.hpp"         // for MATRIX_SCAN_PERIOD_US, DEBOUNCE_*
#include "keymap_thread.hpp"    // for signal_key_event()
#include "matrix_thread.hpp"



matrix_thread::matrix_thread()
{
    m_pthread = thread_get_unchecked( thread_create(
        m_stack, sizeof(m_stack),
        THREAD_PRIO_MATRIX,
        THREAD_CREATE_STACKTEST,
        _matrix_thread, this, "matrix_thread") );

    // Initialize matrix gpio pins and start ISR for detecting GPIO_RISING.
    matrix_init(&_debouncer, &_isr_detect_any_key_down, this);
}

void* matrix_thread::_matrix_thread(void* arg)
{
    matrix_thread* const that = static_cast<matrix_thread*>(arg);

    while ( true ) {
        // Zzz
        if ( thread_flags_wait_any(FLAG_SCAN) )
            that->perform_scan();
    }

    return nullptr;
}

void matrix_thread::_debouncer(unsigned row, unsigned col, bool is_press)
{
    auto& state = obj().m_states[row*MATRIX_COLS + col];
    // Update the event history.
    state.history = (state.history << 1) | is_press;
}

void matrix_thread::perform_scan()
{
    matrix_scan();

    bool any_pressed = false;

    // Notify keymap_thread for each key that has changed its state.
    for ( size_t index = 0 ; index < NUM_SLOTS ; ++index ) {
        auto& state = m_states[index];
        if ( (state.data == DEBOUNCE_PATTERN_FOR_RELEASE) ||
             ((state.data & DEBOUNCE_MASK_FOR_PRESS) == DEBOUNCE_PATTERN_FOR_PRESS) ) {
            const bool to_press = (state.data >= 0);  // == (state.data & 1)
            if ( !keymap_thread::obj().signal_key_event(
                    index, to_press, MATRIX_SCAN_PERIOD_US) ) {
                // A previous press is about to be released or a new press is about to be
                // made but it has failed, retaining its previous state. In either case
                // we can assume that it is (still or yet to be) pressed, which will also
                // prevent matrix_thread from going to sleep.
                any_pressed = true;
                break;
            }
            // Change the state (press or release) only when successfully signaled.
            state.is_press = to_press;
        }

        any_pressed |= (state.data < 0);  // Is it pressed now?
    }

    if ( any_pressed ) {
        m_first_scan.stop();
        ztimer_set_timeout_flag(ZTIMER_USEC,
            &m_scan_timer, MATRIX_SCAN_PERIOD_US - TYPICAL_SCAN_TIME_US);
    }

    // Stay in first_scan until press is detected, performing scans with reduced period.
    // (We know that something has triggered the interrupt though it might be spurious
    // noise.)
    else if ( m_first_scan.in_progress() ) {
        static_assert( MATRIX_SCAN_PERIOD_US / 4 > TYPICAL_SCAN_TIME_US );
        ztimer_set_timeout_flag(ZTIMER_USEC,
            &m_scan_timer, MATRIX_SCAN_PERIOD_US / 4 - TYPICAL_SCAN_TIME_US);
    }

    // When all keys are released we go back to sleep, setting up the interrupt to take
    // over the following detection.
    else {
        m_any_pressed = false;
        matrix_enable_interrupts();
        LOG_DEBUG("Matrix: ---- sleep\n");
    }
}

void matrix_thread::_isr_detect_any_key_down(void* arg)
{
    // Prepare to wake up for the first scan.
    matrix_disable_interrupts();
    matrix_thread* const that = static_cast<matrix_thread*>(arg);
    that->m_any_pressed = true;
    that->m_first_scan.start();
    thread_flags_set(that->m_pthread, FLAG_SCAN);
}
