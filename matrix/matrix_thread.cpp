#include "board.h"              // for THREAD_PRIO_MATRIX
#include "log.h"
#include "matrix.h"             // for matrix_init(), matrix_scan(), ...
#include "mutex.h"              // for mutex_lock(), mutex_unlock()
#include "periph_conf.h"        // for NUM_MATRIX_SLOTS
#include "thread.h"             // for thread_create(), ...
#include "ztimer.h"             // for ztimer_now(), ztimer_periodic_wakeup(), ...

#include "config.hpp"           // for MATRIX_SCAN_PERIOD_US, DEBOUNCE_*, ...
#include "main_thread.hpp"      // for signal_key_event(), signal_thread_idle()
#include "matrix_thread.hpp"



thread_t* matrix_thread::m_pthread = nullptr;

#ifdef DEVELHELP
    char matrix_thread::m_thread_stack[THREAD_STACKSIZE_MEDIUM];
#else
    char matrix_thread::m_thread_stack[THREAD_STACKSIZE_SMALL];
#endif

// Start matrix_thread by sleeping; will wake up on an interrupt.
mutex_t matrix_thread::m_sleep_lock = MUTEX_INIT_LOCKED;

matrix_thread::bounce_state_t matrix_thread::m_states[NUM_MATRIX_SLOTS];

uint32_t matrix_thread::m_wakeup_us = 0;

int matrix_thread::m_min_scan_count = 0;

bool matrix_thread::m_is_polling = false;

void matrix_thread::init()
{
    m_pthread = thread_get_unchecked( thread_create(
        m_thread_stack, sizeof(m_thread_stack),
        THREAD_PRIO_MATRIX,
        THREAD_CREATE_STACKTEST,
        _thread_entry, nullptr, "matrix_thread") );

    // Initialize the matrix GPIO pins and start ISR for detecting GPIO_RISING.
    matrix_init(&_debouncer, &_isr_any_key_down, &m_sleep_lock);
}

void matrix_thread::_debouncer(unsigned mat_index, bool pressing)
{
    bounce_state_t& state = m_states[mat_index];

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
    while ( true ) {
        mutex_lock(&m_sleep_lock);  // Zzz

        matrix_scan();

        uint8_t any_pressed = 0;

        // Notify main_thread of every key state change.
        for ( unsigned mat_index = 0 ; mat_index < NUM_MATRIX_SLOTS ; mat_index++ ) {
            bounce_state_t& state = m_states[mat_index];
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

            any_pressed |= state.value;
        }

        // If any key is pressed or if the minimum scan count hasn't been hit, we
        // continue scanning.
        if ( --m_min_scan_count > 0 || any_pressed ) {
            // ztimer_periodic_wakeup() is used instead of ztimer_set_timeout_flag() to
            // ensure precise sleep duration.
            ztimer_periodic_wakeup(  // Zzz
                ZTIMER_USEC, &m_wakeup_us, MATRIX_SCAN_PERIOD_US);
            mutex_unlock(&m_sleep_lock);
        }

        // Otherwise, we return to sleep and rely on the interrupt to detect the next
        // key press.
        // Note: Level-triggered interrupts are quite reliable and won't miss key presses,
        // even during the re-enablement. Therefore, it's safe to enter sleep as early as
        // possible. However, additional detection within the interrupt handler is
        // required before reading the matrix.
        else {
            m_is_polling = false;
            main_thread::signal_thread_idle();
            // LOG_DEBUG("Matrix: ---------> @%lu\n", ztimer_now(ZTIMER_MSEC));
            ztimer_release(ZTIMER_USEC);
            matrix_enable_interrupt();
            // The thread will sleep until the interrupt releases m_sleep_lock.
        }
    }
}

void matrix_thread::_isr_any_key_down(void* arg)
{
    // Suppress further triggers in case the interrupt is refired while being disabled.
    if ( m_min_scan_count > 0 )
        return;

    // Prepare to wake up for the active scan.
    matrix_disable_interrupt();
    m_min_scan_count = DEBOUNCE_PRESS_MS;
    ztimer_acquire(ZTIMER_USEC);
    m_wakeup_us = ztimer_now(ZTIMER_USEC);
    m_is_polling = true;
    // LOG_DEBUG("Matrix: <--------- @%lu\n", ztimer_now(ZTIMER_MSEC));
    mutex_unlock(static_cast<mutex_t*>(arg));
}
