#include "assert.h"
#include "board.h"              // for _sheap, _eheap, system_reset()
#include "cpu.h"                // for RSTC
#include "event.h"              // for event_queue_init(), event_get(), ...
#include "irq.h"                // for irq_disable(), irq_restore()
#include "log.h"                // for set_log_mask()
#include "periph/wdt.h"         // for wdt_kick()
#include "thread.h"             // for thread_get_active()
#include "thread_flags.h"       // for thread_flags_set(), thread_flags_wait_any()
#include "ztimer.h"             // for ztimer_set_timeout_flag()

#include "adc.hpp"              // for adc::v_5v, v_con1, v_con2
#include "lua.hpp"              // for lua::init()
#include "config.hpp"           // for ENABLE_CDC_ACM, ENABLE_LUA_REPL
#include "key_queue.hpp"        // for key_queue::push(), ...
#include "lkeymap.hpp"          // for lua::handle_key_event()
#include "main_thread.hpp"
#include "matrix_thread.hpp"    // for matrix_thread::init()
#include "persistent.hpp"       // for persistent::init()
#include "repl.hpp"             // for lua::repl::init(), ...
#include "timed_stdin.hpp"      // for timed_stdin::wait_for_input(), ...
#include "usb_thread.hpp"       // for usb_thread::init()
#include "usbhub_thread.hpp"    // for usbhub_thread::init()



// This main() function is called from main_trampoline() in the "main" thread context.
// See kernel_init() for more information.
extern "C" int main()
{
    main_thread::init();
}



thread_t* main_thread::m_pthread;

event_queue_t main_thread::m_event_queue;

NORETURN void main_thread::init()
{
    m_pthread = thread_get_active();

    // Initialize subsystems in the order of dependency.
    persistent::init();  // Initialize NVM. (See `last_host_port` for a usage example.)
    usbhub_thread::init();
    usb_thread::init();  // printf() will work from this point, displaying on the host.
    // rgb::init();
    matrix_thread::init();

    // RGB_LED related code should be in rgb::init().

    // Refer to riot/cpu/sam0_common/include/vendor/samd51/include/component/rstc.h
    // for the meaning of each bit. For instance, 0x40 indicates a system reset.
    // Resets other than a system reset or power reset are not expected, as they are
    // caught by the bootloader.
    LOG_DEBUG("Main: RSTC->RCAUSE.reg=0x%x\n", RSTC->RCAUSE.reg);

    LOG_DEBUG("Main: max heap size is %d bytes\n", &_eheap - &_sheap);

    // Instead of creating a new thread, use the already existing "main" thread.
    (void)_thread_entry(nullptr);
}

void main_thread::set_thread_flags(thread_flags_t flags)
{
    thread_flags_set(m_pthread, flags);
    if constexpr ( ENABLE_LUA_REPL )
        timed_stdin::stop_read();
}

void main_thread::signal_dte_state(bool dte_enabled)
{
    if constexpr ( ENABLE_CDC_ACM )
        set_thread_flags(dte_enabled ? FLAG_DTE_ENABLED : FLAG_DTE_DISABLED);
    if constexpr ( ENABLE_LUA_REPL ) {
        if ( !dte_enabled )
            set_log_mask(0xff);  // Enable all logs when DTE is disconnected.
    }
}

void main_thread::signal_dte_ready(uint8_t log_mask)
{
    if constexpr ( ENABLE_LUA_REPL ) {
        set_thread_flags(FLAG_DTE_READY);
        set_log_mask(log_mask);
    }
}

void main_thread::signal_event(event_t* event)
{
    event_post(&m_event_queue, event);
    if constexpr ( ENABLE_LUA_REPL )
        timed_stdin::stop_read();
}

bool main_thread::signal_key_event(unsigned slot_index, bool is_press, uint32_t timeout_us)
{
    assert( slot_index < NUM_MATRIX_SLOTS );
    // Convert `slot_index` to `slot_index1`, since Lua array indices start from 1.
    unsigned slot_index1 = slot_index + 1;

    LOG_DEBUG("Matrix: %s [%u] @%lu\n",
        press_or_release(is_press), slot_index1, ztimer_now(ZTIMER_MSEC));

    if ( likely(key_queue::push({{ uint8_t(slot_index1), is_press }}, timeout_us)) ) {
        set_thread_flags(FLAG_KEY_EVENT);
        return true;
    }

    LOG_ERROR("Main: key_queue::push() failed\n");

    // If the key event queue is full with all deferred events, recovery is impossible.
    // Trigger a system reset to restore operability.
    if ( unlikely(key_queue::terminal_full()) )
        system_reset();

    // Otherwise, discard surplus events. Presses may be lost, but missed releases will
    // be reissued by the debouncer.
    return false;
}

NORETURN void* main_thread::_thread_entry(void*)
{
    // The event_queue_init() should be called from the queue-owning thread.
    event_queue_init(&m_event_queue);

    lua::init();

    bool has_input = false;
    thread_flags_t flags;
    while ( true ) {
        // Kick the watchdog timer to keep the system alive. If we don't come back here
        // within the watchdog timeout, the system will trigger a watchdog reset.
        wdt_kick();

        if constexpr ( ENABLE_LUA_REPL ) {
            has_input = timed_stdin::wait_for_input(HEARTBEAT_PERIOD_MS);  // Zzz

            flags = thread_flags_clear(
                FLAG_GENERIC_EVENT
                | FLAG_KEY_EVENT
                | FLAG_USB_RESET
                | FLAG_USB_SUSPEND
                | FLAG_USB_RESUME
                | FLAG_DTE_DISABLED
                | FLAG_DTE_ENABLED
                | FLAG_DTE_READY
                | FLAG_TIMEOUT );

            if ( flags & FLAG_DTE_ENABLED )
                LOG_DEBUG("Main: DTE enabled\n");

            // FLAG_DTE_DISABLED is not signaled on switchover, but FLAG_USB_SUSPEND is.
            if ( flags & (FLAG_DTE_DISABLED | FLAG_USB_SUSPEND) ) {
                timed_stdin::disable();
                lua::repl::stop();
                LOG_DEBUG("Main: DTE disabled\n");
            }

            if ( flags & FLAG_DTE_READY ) {
                lua::repl::start();
                timed_stdin::enable();
                LOG_INFO("Main: DTE ready (0x%x)\n", get_log_mask());
            }
        }

        else {
            // Set up wake-up time before going to sleep.
            ztimer_t heartbeat;
            ztimer_set_timeout_flag(ZTIMER_MSEC, &heartbeat, HEARTBEAT_PERIOD_MS);

            flags = thread_flags_wait_any(  // Zzz
                FLAG_GENERIC_EVENT
                | FLAG_KEY_EVENT
                | FLAG_USB_RESET
                | FLAG_USB_SUSPEND
                | FLAG_USB_RESUME
                | FLAG_DTE_DISABLED
                | FLAG_DTE_ENABLED
                | FLAG_DTE_READY
                | FLAG_TIMEOUT );

            if constexpr ( ENABLE_CDC_ACM ) {
                if ( flags & FLAG_DTE_ENABLED )
                    LOG_DEBUG("Main: DTE enabled\n");

                if ( flags & FLAG_DTE_DISABLED )
                    LOG_DEBUG("Main: DTE disabled\n");
            }

            // Do not process FLAG_DTE_READY when ENABLE_LUA_REPL is false. The stdin
            // remains permanently disabled.
        }

        // Internal key events such as key timeout event and lamp state event are handled
        // all at once through m_event_queue.
        if ( flags & FLAG_GENERIC_EVENT ) {
            while ( event_t* event = event_get(&m_event_queue) )
                event->handler(event);
        }

        // External key events from key_queue are handled one at a time.
        key_queue::entry_t event;
        if ( key_queue::get(&event) ) {
            lua::handle_key_event(event.slot_index1, event.is_press);

            unsigned state = irq_disable();
            m_pthread->flags |= FLAG_KEY_EVENT;
            irq_restore(state);
            // Or could use `atomic_fetch_or_u16(&m_pthread->flags, FLAG_KEY_EVENT)`.

            // Any remaining key events will be handled next time without sleeping.
            continue;
        }

        if constexpr ( ENABLE_LUA_REPL ) {
            // Execute REPL once all events are handled and if input is available. Once
            // has_input is true, all remaining inputs for the compiled Lua chunk are
            // handled within lua::repl::execute().
            if ( has_input )
                lua::repl::execute();
        }

        // if ( flags & FLAG_TIMEOUT ) {
        //     LOG_DEBUG("Main: v_5v=%d v_con1=%d v_con2=%d fsmstatus=0x%x @%lu\n",
        //         adc::v_5v.read(), adc::v_con1.read(), adc::v_con2.read(),
        //         usb_thread::fsmstatus(),
        //         ztimer_now(ZTIMER_MSEC));
        // }
    }
}
