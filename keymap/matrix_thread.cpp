#include "board.h"              // for THREAD_PRIO_MATRIX
#include "log.h"
#include "matrix.h"             // for matrix_init(), matrix_scan(), ...
#include "thread_flags.h"       // for thread_flags_t, thread_flags_set(), ...
#include "ztimer.h"             // for ztimer_now() and ztimer_set_timeout_flag()

#include "features.hpp"         // for DEBOUNCE_TIME_MS and MATRIX_SCAN_PERIOD_US
#include "keymap_thread.hpp"    // for signal_key_event()
#include "manager.hpp"          // for pressing_slot_t::m_when_release_started
#include "matrix_thread.hpp"
#include "pmap.hpp"             // for key::maps[]



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

bool matrix_thread::_debouncer(unsigned row, unsigned col, bool is_pressed)
{
    key::pmap_t& pmap = key::maps_2d[row][col];

    if ( pmap.get_pressing_slot() == nullptr ) {
        if ( is_pressed ) {
            // Note that pressing slots may not be created (or removed) immediately after
            // signaling the press (or release) event to keymap_thread. And, we can
            // possibly signal the same event twice (especially for release, as the
            // pressing slot gets removed at the end, not start, of processing the
            // release event), but such duplicate events will be taken care of by
            // keymap_thread (See manager_t::handle_press/release()).
            LOG_DEBUG("Matrix: press (%p)\n", &pmap);
            keymap_thread::obj().signal_key_event(&pmap, true);
        }
        return is_pressed;
    }

    uint32_t& release_started = pmap.get_pressing_slot()->m_when_release_started;

    if ( is_pressed ) {
        if ( release_started ) {
            LOG_DEBUG("Matrix: press debounced (%p)\n", &pmap);
            release_started = 0;
        }
    }

    else {
        const uint32_t now = ztimer_now(ZTIMER_MSEC);

        if ( !release_started ) {
            // Very rare but it is possible to still have release_started = 0 from
            // ztimer_now() above even though we meant to start releasing. Even so, it is
            // not a problem at all; we only missed the very first scan of the release and
            // count it as being still pressed, which happens quite often due to the
            // nature of bouncing switches.
            release_started = now;
        }

        else if ( now - release_started >= DEBOUNCE_TIME_MS ) {
            LOG_DEBUG("Matrix: release (%p)\n", &pmap);
            // This reset of release_started helps to not send the same release event
            // again to keymap_thread. The released key will be just regarded as being
            // released now and will take another DEBOUNCE_TIME_MS before signaling the
            // same release again, but it will likely soon disappear from the pressing
            // list before ever doing so.
            release_started = now;
            keymap_thread::obj().signal_key_event(&pmap, false);
            return false;
        }
    }

    return true;
}

void matrix_thread::perform_scan()
{
    const bool any_pressed = matrix_scan();

    if ( any_pressed ) {
        m_first_scan = false;
        ztimer_set_timeout_flag(ZTIMER_USEC, &m_scan_timer, MATRIX_SCAN_PERIOD_US);
    }

    // Stay in first_scan until press is detected, scanning with reduced scan period.
    // (This is because the first scan following the interrupt is not reliable as there
    // tends to be a lot of bounces, depending on when the scan is performed, whether
    // during a bounce or not.)
    else if ( m_first_scan ) {
        ztimer_set_timeout_flag(ZTIMER_USEC, &m_scan_timer, MATRIX_SCAN_PERIOD_US / 4);
    }

    // When all keys are released we go back to sleep, setting up the interrupt to take
    // over the following detection.
    else {
        matrix_enable_interrupts();
        LOG_DEBUG("Matrix: ---- sleep\n");
    }
}

void matrix_thread::_isr_detect_any_key_down(void* arg)
{
    // Prepare to wake up for the first scan.
    matrix_disable_interrupts();
    matrix_thread* const that = static_cast<matrix_thread*>(arg);
    that->m_first_scan = true;
    thread_flags_set(that->m_pthread, FLAG_SCAN);
}
