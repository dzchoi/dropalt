#include "board.h"              // for THREAD_PRIO_MATRIX
#include "hid_keycodes.hpp"     // for reporting KC_CAPSLOCK (debugging)

#define ENABLE_DEBUG 1
#include "debug.h"

#include "matrix_thread.hpp"
#include "usb_thread.hpp"       // for reporting KC_CAPSLOCK (debugging)



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
    // Prepare for scanning.
    matrix_disable_interrupts();
    // Clear any more FLAG_START_SCAN that might have arrived while disabling interrupts.
    thread_flags_clear(FLAG_START_SCAN);

    // The first scan following the interrupt is very unreliable as there tends to be a
    // lot of bounces. It depends on when the scan is performed, either during a bounce
    // or not. Ignore the measurement for now but defer the determination at the next
    // scan. (Deferred algorithm)
    (void)matrix_scan(raw_matrix);

    scan_timer.start();
    debounce_timer.start();
    usb_thread::obj().hid_keyboard.press(KC_CAPSLOCK);  // debugging
}

void matrix_thread::continue_scan()
{
    const bool is_changed = matrix_scan(raw_matrix);

    if ( is_changed ) {
        debounce_timer.start();
        DEBUG("Matrix: changed\n");
    }

    // Do nothing if !changed and !debounce_timer.is_running(), which is when a key press
    // was reported and still held down.
}

inline std::pair<bool, bool> matrix_thread::matrix_copy()
{
    // Optimized extensively to be able to reach matrix_enable_interrupts() quickly in
    // debounce_done().
    matrix_row_t _or = 0;
    matrix_row_t _xor = 0;

    for ( unsigned row = 0 ; row < MATRIX_ROWS ; row++ ) {
        _or  |= raw_matrix[row];
        _xor |= matrix[row] ^ raw_matrix[row];
        matrix[row] = raw_matrix[row];
    }

    return std::make_pair(_xor != 0, _or == 0);  // return (is_changed, is_all_zero)
}

void matrix_thread::debounce_done()
{
    bool is_changed, is_all_zero;
    std::tie(is_changed, is_all_zero) = matrix_copy();

    if ( is_changed )
        // Report the change!
        DEBUG("Matrix: report %x %x %x %x %x\n",
            matrix[0], matrix[1], matrix[2], matrix[3], matrix[4]);
    else
        DEBUG("Matrix: --- no report as false change\n");

    // When everything is reported and all keys are up go back to sleep, having the
    // interrupt taking over the following detection. If !is_changed, we stay awake
    // because we know that something was changing to initiate the debounce processing,
    // but not yet detected until the processing is done. Wait for it more.
    if ( is_changed && is_all_zero ) {
        matrix_enable_interrupts();
        scan_timer.stop();
        // Clear any FLAG_SCAN_TIMEOUT that might be set before/during scan_timer.stop().
        thread_flags_clear(FLAG_SCAN_TIMEOUT);
        usb_thread::obj().hid_keyboard.release(KC_CAPSLOCK);  // debugging
        DEBUG("--- done\n");
    }
}

void matrix_thread::_isr_detect_any_key_down(void* arg)
{
    // It can be called multiple times even before/while disabling interrupts as first
    // thing in start_scan(), as quickly as debounces occur.
    matrix_thread* const that = static_cast<matrix_thread*>(arg);
    thread_flags_set(that->m_pthread, FLAG_START_SCAN);
}
