#include "board.h"              // for THREAD_PRIO_MATRIX
#include "ztimer.h"             // for ztimer_now() and ztimer_set_timeout_flag()

#define ENABLE_DEBUG 0
#include "debug.h"

#include "features.hpp"         // for MATRIX_SCAN_PERIOD_MS
#include "keymap_thread.hpp"    // for signal_key_event()
#include "matrix_thread.hpp"
#include "pmap.hpp"             // for key::maps[][]
#include "rgb_thread.hpp"       // for signal_key_event()



matrix_thread::matrix_thread()
{
    // Initialize matrix state: all keys off
    __builtin_memset(matrix, 0, sizeof(matrix));
    __builtin_memset(raw_matrix, 0, sizeof(raw_matrix));

    m_pthread = thread_get_unchecked( thread_create(
        m_stack, sizeof(m_stack),
        THREAD_PRIO_MATRIX,
        THREAD_CREATE_STACKTEST,
        _matrix_thread, this, "matrix_thread") );

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
            | FLAG_CONTINUE_SCAN
            );

        if ( flags & FLAG_START_SCAN )
            that->detect_change(true);

        if ( flags & FLAG_CONTINUE_SCAN )
            that->detect_change(false);
    }

    return nullptr;
}

void matrix_thread::detect_change(bool first_scan)
{
    // Note that the first scan following the interrupt is not reliable as there tends to
    // be a lot of bounces. It depends on when the scan is performed, whether during a
    // bounce or not.
    const bool is_changed = matrix_scan(raw_matrix) || first_scan;
    bool continue_scan = true;

    if ( is_changed ) {
        // Very rare but it is possible to have debounce_started = 0 from ztimer_now()
        // even though we meant to start debounce. Even so, it is not a problem at all,
        // being regarded as missing one true change, which happens quite often during
        // scans.
        debounce_started = ztimer_now(ZTIMER_MSEC);
        DEBUG("Matrix: changed\t\t%x %x %x %x %x\n",
            raw_matrix[0], raw_matrix[1], raw_matrix[2], raw_matrix[3], raw_matrix[4]);
    }

    else if ( is_debounce_done() ) {
        continue_scan = commit_change();
        debounce_started = 0;
    }

    if ( continue_scan )
        ztimer_set_timeout_flag(ZTIMER_MSEC, &scan_timer, MATRIX_SCAN_PERIOD_MS);
}

bool matrix_thread::commit_change()
{
    // Update matrix[] and report any changes from the last report.
    matrix_row_t is_any_down = 0;
    bool is_changed = false;
    for ( unsigned row = 0 ; row < MATRIX_ROWS ; row++ ) {
        is_any_down |= raw_matrix[row];
        if ( matrix_row_t _xor = matrix[row] ^ raw_matrix[row] ) {
            do {
                const unsigned col = __builtin_ctz(_xor);
                const matrix_row_t mask = matrix_row_t(1) << col;
                const bool is_pressed = ((raw_matrix[row] & mask) != 0);
                DEBUG("Matrix: report %s\n", is_pressed ? "press" : "release");
                key::pmap_t& pmap = key::maps[row][col];
                rgb_thread::obj().signal_key_event(pmap.led_id(), is_pressed);
                keymap_thread::obj().signal_key_event(&pmap, is_pressed);
                _xor &= ~mask;
            } while ( _xor != 0 );
            matrix[row] = raw_matrix[row];
            is_changed = true;
        }
    }

    if ( !is_changed ) {
        // If !is_changed, we stay awake because we know that something was changing to
        // initiate the debouncing process, but not yet detected until the debouncing
        // is done. Wait for it further.
        DEBUG("Matrix:\e[0;31m --- no report\e[0m\n");
        return true;
    }

    if ( is_any_down ) {
        DEBUG("Matrix: still pressed\t\t%x %x %x %x %x\n",
            raw_matrix[0], raw_matrix[1], raw_matrix[2], raw_matrix[3], raw_matrix[4]);
        return true;
    }

    // When the matrix has been changed and all keys are up, we go back to sleep, setting
    // up the interrupt to take over the following detection.
    matrix_enable_interrupts();
    DEBUG("Matrix: --- done\n");
    return false;  // to stop scan.
}

void matrix_thread::_isr_detect_any_key_down(void* arg)
{
    // Prepare to wake up for the first scan.
    matrix_disable_interrupts();
    matrix_thread* const that = static_cast<matrix_thread*>(arg);
    thread_flags_set(that->m_pthread, FLAG_START_SCAN);
}