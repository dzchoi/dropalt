#include "board.h"              // for THREAD_PRIO_MATRIX
#include "hid_keycodes.hpp"

#define ENABLE_DEBUG 1
#include "debug.h"

#include "keymap.hpp"
#include "matrix_thread.hpp"
#include "usb_thread.hpp"



matrix_thread::matrix_thread():
    m_pthread {
        thread_get( thread_create(
            m_stack, sizeof(m_stack),
            THREAD_PRIO_MATRIX,
            THREAD_CREATE_STACKTEST,
            _matrix_thread, this, "matrix_thread") )
    },
    scan_timer { MATRIX_SCAN_PERIOD_US, m_pthread },
    debounce_timer { DEBOUNCE_TIME_US, m_pthread, FLAG_DEBOUNCE_TIMEOUT }
{
    // Initialize matrix state: all keys off
    __builtin_memset(matrix, 0, sizeof(matrix));
    __builtin_memset(raw_matrix, 0, sizeof(raw_matrix));

    // Initialize matrix gpio pins and start ISR for detecting GPIO_RISING.
    matrix_init(&_isr_detect_any_key_down, this);
}

void* matrix_thread::_matrix_thread(void* arg)
{
    matrix_thread* const that = static_cast<matrix_thread*>(arg);

    while ( true ) {
        // Zzz
        thread_flags_t flags = thread_flags_wait_any(
            FLAG_EVENT
            | FLAG_START_SCAN
            | FLAG_DEBOUNCE_TIMEOUT
            | FLAG_SCAN_TIMEOUT
            );

        if ( flags & FLAG_START_SCAN )
            that->start_scan();

        // FLAG_DEBOUNCE_TIMEOUT has higher priority than FLAG_SCAN_TIMEOUT does.
        else if ( flags & FLAG_DEBOUNCE_TIMEOUT )
            that->debounce_done();

        else if ( flags & FLAG_SCAN_TIMEOUT )
            that->continue_scan();
    }

    return nullptr;
}

void matrix_thread::start_scan()
{
    // The first scan following the interrupt is very unreliable as there tends to be a
    // lot of bounces. It depends on when the scan is performed, either during a bounce
    // or not. Ignore the measurement for now but defer the determination at the next
    // scan. (Deferred algorithm)
    (void)matrix_scan(raw_matrix);
    DEBUG("Matrix: pressed\t%x %x %x %x %x\n",
        raw_matrix[0], raw_matrix[1], raw_matrix[2], raw_matrix[3], raw_matrix[4]);

    scan_timer.start();
    debounce_timer.start();
}

void matrix_thread::continue_scan()
{
    const bool is_changed = matrix_scan(raw_matrix);
    if ( is_changed ) {
        debounce_timer.start();
        DEBUG("Matrix: changed\t%x %x %x %x %x\n",
            raw_matrix[0], raw_matrix[1], raw_matrix[2], raw_matrix[3], raw_matrix[4]);
    }
}

void matrix_thread::debounce_done()
{
    // Find the difference per each row since last report. However, We do not report
    // the change right now in order to enable the interrupt below as soon as possible.
    matrix_row_t _or_all = 0;
    matrix_row_t _xor_all = 0;
    for ( unsigned row = 0 ; row < MATRIX_ROWS ; row++ ) {
        _or_all |= raw_matrix[row];
        _xor_all |= (matrix[row] ^= raw_matrix[row]);
    }

    const bool is_changed = (_xor_all != 0);
    if ( !is_changed ) {
        // If !is_changed, we stay awake because we know that something was changing to
        // initiate the debouncing process, but not yet detected until the debouncing
        // is done. Wait for it further.
        DEBUG("Matrix: --- no report\t%x %x %x %x %x\n",
            raw_matrix[0], raw_matrix[1], raw_matrix[2], raw_matrix[3], raw_matrix[4]);
        return;
    }

    // When the matrix is changed and all keys are up, we prepare to go back to sleep,
    // setting up the interrupt to take over the following detection.
    const bool is_all_zero = (_or_all == 0);
    if ( is_all_zero ) {
        matrix_enable_interrupts();
        scan_timer.stop();
        // Clear any FLAG_SCAN_TIMEOUT that might be set before/during scan_timer.stop().
        // Otherwise, at the following continue_scan() all matrix column pins will be
        // unselected, causing the interrupt never triggered again.
        thread_flags_clear(FLAG_SCAN_TIMEOUT);
    }

    // Update matrix[] and report the change. We do it now as it is fine to start the
    // following first scan a little late as the scan will be likely unreliable.
    for ( unsigned row = 0 ; row < MATRIX_ROWS ; row++ ) {
        matrix_row_t _xor = matrix[row];  // has the difference found above.
        while ( _xor != 0 ) {
            const unsigned col = __builtin_ctz(_xor);
            const matrix_row_t mask = matrix_row_t(1) << col;
            const bool is_pressed = ((raw_matrix[row] & mask) != 0);
            usb_thread::obj().hid_keyboard.key_event(keymap[row][col], is_pressed);
            DEBUG("Matrix: reported key (0x%x) %s\n", keymap[row][col],
                is_pressed ? "press" : "release");
            _xor &= ~mask;
        }
        matrix[row] = raw_matrix[row];
    }

    if ( is_all_zero ) {
        DEBUG("--- done\n");
        // Bug!: Without the following trivial statement debounce_done() crashes at the
        //  first key press and release. I don't know why. Alternatively, we can do:
        //   - do not call hid_keyboard.key_event() above,
        //   - wrap the accessing of `bits` in usbus_hid_keyboard_t::help_update_bits()
        //      with irq_disable() and irq_restore(),
        //  But, xtimer_xsleep() does not work.
        // Symptom:
        //   - At first release, sw reset is hit in hid_keyboard.key_event(), in
        //     usbus_hid_keyboard_t::help_update_bits().
        //   - Before release, if we hold the key down, watchdog reset is caused while
        //     matrix_thread is running the scan_timer and continuous_scan().
        //   - This trivial statement seems to get rid of the problem, but why?
        // do_not_die = true;
    }
}

void matrix_thread::_isr_detect_any_key_down(void* arg)
{
    // Prepare to wake up for the first scan.
    matrix_disable_interrupts();
    matrix_thread* const that = static_cast<matrix_thread*>(arg);
    thread_flags_set(that->m_pthread, FLAG_START_SCAN);
}
