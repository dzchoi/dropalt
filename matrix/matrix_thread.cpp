#include "board.h"              // for THREAD_PRIO_MATRIX
#include "irq.h"                // for irq_disable(), irq_restore()
#include "matrix.h"             // for matrix_init(), matrix_scan(), ...
#include "periph_conf.h"        // for NUM_MATRIX_SLOTS
#include "thread.h"             // for thread_create(), sched_set_status(), ...
#include "ztimer.h"             // for ztimer_now(), ztimer_periodic_wakeup(), ...

#include "config.hpp"           // for MATRIX_SCAN_PERIOD_US, DEBOUNCE_*, ...
#include "main_thread.hpp"      // for signal_key_event(), signal_thread_idle()
#include "matrix_thread.hpp"



thread_t* matrix_thread::m_pthread = nullptr;

bool matrix_thread::m_enabled = false;

alignas(8) char matrix_thread::m_thread_stack[MATRIX_STACKSIZE];

matrix_thread::bounce_state_t matrix_thread::m_key_states[NUM_MATRIX_SLOTS];

uint32_t matrix_thread::m_wakeup_us = 0;

int matrix_thread::m_min_scan_count = 0;

void matrix_thread::init()
{
    m_pthread = thread_get_unchecked( thread_create(
        m_thread_stack, sizeof(m_thread_stack),
        THREAD_PRIO_MATRIX,
        THREAD_CREATE_SLEEPING | THREAD_CREATE_STACKTEST,
        _thread_entry, nullptr, "matrix_thread") );

    m_enabled = true;

    // Initialize the matrix GPIO pins and start ISR for detecting GPIO_RISING.
    matrix_init(&_debouncer, &_isr_any_key_down, nullptr);
}

void matrix_thread::_debouncer(unsigned mat_index, bool pressing)
{
    bounce_state_t& state = m_key_states[mat_index];

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

// Convert `mat_index` to `slot_index`, skipping over unused indices and adding +1 to
// align with Lua's 1-based array indexing.
static inline unsigned map_index(unsigned mat_index)
{
    // 0  -> 1
    // ...
    // 41 -> 42
    //
    // 42 -> X
    // 43 -> 43
    // 44 -> 44
    // 45 -> 45
    //
    // 46 -> X
    // 47 -> 46
    // ...
    // 62 -> 61
    //
    // 63 -> X
    // 64 -> X
    // 65 -> X
    // 66 -> 62  (SPACE)
    //
    // 67 -> X
    // 68 -> X
    // 69 -> X
    // 70 -> 63
    // ...
    // 74 -> 67

    // Branchless code:
    // return mat_index
    //     + (mat_index <  UNUSED_MATRIX_INDICES[0])   // 42
    //     - (mat_index >= UNUSED_MATRIX_INDICES[1])   // 46
    //     - (mat_index >= UNUSED_MATRIX_INDICES[2])   // 63
    //     - (mat_index >= UNUSED_MATRIX_INDICES[3])   // 64
    //     - (mat_index >= UNUSED_MATRIX_INDICES[4])   // 65
    //     - (mat_index >= UNUSED_MATRIX_INDICES[5])   // 67
    //     - (mat_index >= UNUSED_MATRIX_INDICES[6])   // 68
    //     - (mat_index >= UNUSED_MATRIX_INDICES[7]);  // 69

    // Branching code:
    if ( mat_index < UNUSED_MATRIX_INDICES[0] )  // 42
        return mat_index + 1;
    if ( mat_index < UNUSED_MATRIX_INDICES[1] )  // 46
        return mat_index;
    if ( mat_index < UNUSED_MATRIX_INDICES[2] )  // 63
        return mat_index - 1;
    if ( mat_index < UNUSED_MATRIX_INDICES[5] )  // 67
        return 62;  // Or mat_index - 4;
    return mat_index - 7;
}

NORETURN void* matrix_thread::_thread_entry(void*)
{
    // Note that this thread is created with THREAD_CREATE_SLEEPING and remains sleeping
    // until an interrupt occurs.
    while ( true ) {
        matrix_scan();

        uint8_t any_pressed = 0;

        // Notify main_thread of every key state change.
        for ( unsigned mat_index = 0 ; mat_index < NUM_MATRIX_SLOTS ; mat_index++ ) {
            bounce_state_t& state = m_key_states[mat_index];
            if ( state.pressing != state.pressed ) {
                if ( !main_thread::signal_key_event(
                  map_index(mat_index), state.pressing, MATRIX_SCAN_PERIOD_US) ) {
                    // If a key state change signal to main_thread fails, the key retains
                    // its previous state, and remains considered (still or yet to be)
                    // pressed to prevent matrix_thread from going to sleep.
                    any_pressed = 1;
                    break;
                }
                // Change the state (press or release) only when successfully signaled.
                state.pressed = state.pressing;
            }

            any_pressed |= state.uint8;
        }

        // If any key is pressed or if the minimum scan count hasn't been hit, we
        // continue scanning.
        if ( --m_min_scan_count > 0 || any_pressed ) {
            // ztimer_periodic_wakeup() is used instead of ztimer_set_timeout_flag() to
            // ensure precise sleep duration.
            ztimer_periodic_wakeup(  // Zzz
                ZTIMER_USEC, &m_wakeup_us, MATRIX_SCAN_PERIOD_US);
        }

        // Otherwise, we return to sleep and rely on the interrupt to detect the next
        // key press.
        // Note: Level-triggered interrupts are quite reliable and won't miss key presses,
        // even during the re-enablement. Therefore, it's safe to enter sleep as early as
        // possible. However, additional detection within the interrupt handler is
        // required before reading the matrix.
        else {
            main_thread::signal_thread_idle();
            // LOG_DEBUG("Matrix: ---------> @%lu", ztimer_now(ZTIMER_MSEC));
            ztimer_release(ZTIMER_USEC);

            // This code is the same as thread_sleep(), only matrix_enable_interrupt()
            // is added inside a critical section.
            unsigned state = irq_disable();
            matrix_enable_interrupt();
            sched_set_status(m_pthread, STATUS_SLEEPING);
            irq_restore(state);
            thread_yield_higher();  // Zzz
        }
    }
}

void matrix_thread::_isr_any_key_down(void*)
{
    // Checking `m_min_scan_count > 0` prevents retriggering if the interrupt fires
    // again during this ISR.
    if ( !m_enabled || m_min_scan_count > 0 )
        return;

    // Prepare to wake up for the active scan.
    m_min_scan_count = DEBOUNCE_PRESS_MS;  // > 0
    matrix_disable_interrupt();
    ztimer_acquire(ZTIMER_USEC);
    m_wakeup_us = ztimer_now(ZTIMER_USEC);
    // LOG_DEBUG("Matrix: <--------- @%lu", ztimer_now(ZTIMER_MSEC));
    thread_wakeup(thread_getpid_of(m_pthread));
}
