#include "board.h"              // for THREAD_PRIO_MATRIX
#include "log.h"
#include "matrix.h"             // for matrix_init(), matrix_scan(), ...
#include "periph_conf.h"        // for NUM_MATRIX_SLOTS
#include "thread.h"             // for thread_create(), thread_sleep(), ...
#include "ztimer.h"             // for ztimer_now(), ztimer_periodic_wakeup(), ...

#include "config.hpp"           // for MATRIX_SCAN_PERIOD_US, DEBOUNCE_*, ...
#include "main_thread.hpp"      // for signal_key_event()
#include "matrix_thread.hpp"



thread_t* matrix_thread::m_pthread = nullptr;

#ifdef DEVELHELP
    char matrix_thread::m_thread_stack[THREAD_STACKSIZE_MEDIUM];
#else
    char matrix_thread::m_thread_stack[THREAD_STACKSIZE_SMALL];
#endif

matrix_thread::bounce_state_t matrix_thread::m_states[NUM_MATRIX_SLOTS];

uint32_t matrix_thread::m_wakeup_us = 0;

unsigned matrix_thread::m_first_scan = 0;

void matrix_thread::init()
{
    m_pthread = thread_get_unchecked( thread_create(
        m_thread_stack, sizeof(m_thread_stack),
        THREAD_PRIO_MATRIX,
        THREAD_CREATE_SLEEPING | THREAD_CREATE_STACKTEST,
        _thread_entry, nullptr, "matrix_thread") );

    // Initialize the matrix GPIO pins and start ISR for detecting GPIO_RISING.
    matrix_init(&_debouncer, &_isr_any_key_down, nullptr);
}

void matrix_thread::_debouncer(unsigned slot_index, bool pressing)
{
    bounce_state_t& state = m_states[slot_index];

    if ( pressing ) {
        if ( state.pressing ) {
            if ( state.counter < DEBOUNCE_RELEASE_MS )
                state.counter++;
        }

        else {
            if ( state.counter < DEBOUNCE_PRESS_MS ) {
                state.counter++;
                if ( state.counter == DEBOUNCE_PRESS_MS ) {
                    state.counter = DEBOUNCE_RELEASE_MS;
                    state.pressing = true;
                }
            }
        }
    }

    else {
        if ( state.counter > 0 ) {
            state.counter--;
            if ( state.counter == 0 )
                state.pressing = false;
        }
    }
}

NORETURN void* matrix_thread::_thread_entry(void*)
{
    // Note that this thread is created with THREAD_CREATE_SLEEPING and remains sleeping
    // until an interrupt occurs.
    while ( true ) {
        matrix_scan();

        uint8_t any_pressed = 0;

        // Notify main_thread of every key state change.
        for ( unsigned slot_index = 0 ; slot_index < NUM_MATRIX_SLOTS ; slot_index++ ) {
            bounce_state_t& state = m_states[slot_index];
            if ( state.pressing != state.pressed ) {
                if ( !main_thread::signal_key_event(
                        slot_index, state.pressing, MATRIX_SCAN_PERIOD_US) ) {
                    // If a key state change signal to main_thread fails, the key retains
                    // its previous state, and remains considered (still or yet to be)
                    // pressed to prevent matrix_thread from going to sleep.
                    any_pressed = 1;
                    break;
                }
                // Change the state (press or release) only when successfully signaled.
                state.pressed = state.pressing;
            }

            any_pressed |= state.value;
        }

        if ( any_pressed ) {
            m_first_scan = 0;
            // ztimer_periodic_wakeup() is used instead of ztimer_set_timeout_flag() to
            // ensure precise sleep duration.
            ztimer_periodic_wakeup(  // Zzz
                ZTIMER_USEC, &m_wakeup_us, MATRIX_SCAN_PERIOD_US);
        }

        // Stay in the first scan until a press is detected, scanning the matrix
        // continuously (for 8 ms) rather than periodically. (We know that an interrupt
        // has been triggered though it may be due to spurious noise.)
        else if ( m_first_scan > 0u ) {
            m_first_scan--;  // Loop repeats if m_first_scan == 0.
        }

        // When all keys are released we go back to sleep, allowing the interrupt to
        // handle subsequent key detection.
        else {
            LOG_DEBUG("Matrix: ---- sleep\n");
            ztimer_release(ZTIMER_USEC);
            matrix_enable_interrupt();
            thread_sleep();  // Zzz
        }
    }
}

void matrix_thread::_isr_any_key_down(void*)
{
    // Prepare to wake up for the first scan.
    matrix_disable_interrupt();
    static_assert( MATRIX_FIRST_SCAN_MAX_COUNT >= 1 );
    m_first_scan = MATRIX_FIRST_SCAN_MAX_COUNT - 1;
    ztimer_acquire(ZTIMER_USEC);
    m_wakeup_us = ztimer_now(ZTIMER_USEC);
    thread_wakeup(thread_getpid_of(m_pthread));
}
