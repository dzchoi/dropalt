#include "assert.h"
#include "board.h"              // for _sheap, _eheap, system_reset()
#include "cpu.h"                // for RSTC
#include "irq.h"                // for irq_disable(), irq_restore()
#include "led_conf.h"           // for KEY_LED_COUNT
#include "log.h"                // for set_log_mask()
#include "periph/wdt.h"         // for wdt_kick()
#include "thread.h"             // for thread_get_active()
#include "thread_flags.h"       // for thread_flags_set(), thread_flags_wait_any()
#include "ztimer.h"             // for ztimer_set_timeout_flag()

#include "adc.hpp"              // for adc::init()
#include "event_ext.hpp"        // for event_post(), event_queue_init(), event_get()
#include "lua.hpp"              // for lua::init()
#include "config.hpp"           // for ENABLE_CDC_ACM, ENABLE_LUA_REPL
#include "key_queue.hpp"        // for key_queue::push(), ...
#include "lexecute.hpp"         // for lua::execute_pending_calls(), ...
#include "lkeymap.hpp"          // for lua::handle_key_event(), lua::handle_lamp_state()
#include "main_thread.hpp"
#include "matrix_thread.hpp"    // for matrix_thread::init(), matrix_thread::is_idle()
#include "persistent.hpp"       // for persistent::init()
#include "repl.hpp"             // for lua::repl::init(), ...
#include "timed_stdin.hpp"      // for timed_stdin::wait_for_input(), ...
#include "usb_thread.hpp"       // for usb_thread::init(), usb_thread::is_idle()
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
    adc::init();         // Invokes v_5v.wait_for_stable_5v().
    persistent::init();  // Initialize NVM. (See `last_host_port` for a usage example.)
    usbhub_thread::init();
    usb_thread::init();  // printf() will work from this point, displaying on the host.
    matrix_thread::init();  // Produces signals to main_thread.

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
        timed_stdin::stop_wait();
}

void main_thread::adjust_my_flags(thread_flags_t flags, bool enable)
{
    // assert( is_active() );  // Ensure that it is called from main_thread.
    unsigned state = irq_disable();
    m_pthread->flags = (m_pthread->flags & ~flags) | (-(int)enable & flags);
    // Alternatively,
    // if ( enable )
    //     m_pthread->flags |= flags;
    // else
    //     m_pthread->flags &= ~flags;
    irq_restore(state);
    // Or could use `atomic_fetch_or_u16(&m_pthread->flags, flags)`.
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
        timed_stdin::stop_wait();
}

bool main_thread::signal_key_event(unsigned slot_index, bool is_press, uint32_t timeout_us)
{
    assert( slot_index <= KEY_LED_COUNT );
    LOG_DEBUG("Matrix: %s [%u] @%lu\n",
        press_or_release(is_press), slot_index, ztimer_now(ZTIMER_MSEC));

    if ( likely(key_queue::push({{ uint8_t(slot_index), is_press }}, timeout_us)) ) {
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

void main_thread::signal_lamp_state(uint_fast8_t lamp_state)
{
    static event_ext_t<uint_fast8_t> _event = { nullptr,
        [](event_t* event) {
            lua::handle_lamp_state(static_cast<event_ext_t<uint_fast8_t>*>(event)->arg);
        },
        0
    };

    _event.arg = lamp_state;
    signal_event(&_event);
}

void main_thread::preset_flags()
{
    adjust_my_flags(FLAG_KEY_EVENT, key_queue::get());

    adjust_my_flags(FLAG_PENDING_CALLS, lua::execute_is_pending());

    if constexpr ( ENABLE_LUA_REPL ) {
        // Note that if FLAG_KEY_EVENT or FLAG_PENDING_CALLS is set above,
        // wait_for_input() returns immediately without waiting for an input.
        adjust_my_flags(FLAG_EXECUTE_REPL,
            timed_stdin::wait_for_input(HEARTBEAT_PERIOD_MS));  // Zzz
    }
    else {
        // Set up wake-up time before going to sleep.
        static ztimer_t heartbeat;
        ztimer_set_timeout_flag(ZTIMER_MSEC, &heartbeat, HEARTBEAT_PERIOD_MS);
    }
}

NORETURN void* main_thread::_thread_entry(void*)
{
    // The event_queue_init() should be called from the queue-owning thread.
    event_queue_init(&m_event_queue);

    // Initialize keymaps and LED effect.
    lua::init();

    while ( true ) {
        // Kick the watchdog timer to keep the system alive. If we don't come back here
        // within the watchdog timeout, the system will trigger a watchdog reset.
        wdt_kick();

        // Preset flags for the conditions that do not trigger signaling.
        preset_flags();

        // Note that if any flag is set via preset_flags(), thread_flags_wait_one()
        // returns immediately with that flag (or a higher-priority one), without
        // sleeping.
        thread_flags_t flag = thread_flags_wait_one(
            FLAG_GENERIC_EVENT
            | FLAG_USB_RESET
            | FLAG_USB_SUSPEND
            | FLAG_USB_RESUME
            | FLAG_DTE_DISABLED
            | FLAG_DTE_ENABLED
            | FLAG_DTE_READY
            | FLAG_KEY_EVENT
            | FLAG_PENDING_CALLS
            | FLAG_EXECUTE_REPL
            | FLAG_TIMEOUT );

        switch ( flag ) {
            case FLAG_GENERIC_EVENT:
                // Key timeout and lamp events are prioritized and processed first via
                // m_event_queue.
                while ( event_t* event = event_get(&m_event_queue) )
                    event->handler(event);
                break;

            case FLAG_DTE_ENABLED:
                if constexpr ( ENABLE_CDC_ACM )
                    LOG_DEBUG("Main: DTE enabled\n");
                break;

            case FLAG_DTE_DISABLED:
                if constexpr ( ENABLE_LUA_REPL ) {
                    if ( timed_stdin::is_enabled() ) {
                        lua::repl::stop();
                        timed_stdin::disable();
                        LOG_DEBUG("Main: REPL disabled\n");
                    }
                }
                if constexpr ( ENABLE_CDC_ACM )
                    LOG_DEBUG("Main: DTE disabled\n");
                break;

            case FLAG_DTE_READY:
                if constexpr ( ENABLE_LUA_REPL ) {
                    if ( !timed_stdin::is_enabled() ) {
                        timed_stdin::enable();
                        lua::repl::start();
                        LOG_INFO("Main: REPL ready (0x%x)\n", get_log_mask());
                    }
                }
                // Do not process FLAG_DTE_READY when ENABLE_LUA_REPL is false. The
                // stdin remains permanently disabled.
                break;

            case FLAG_KEY_EVENT:
                // Key (press/release) events from key_queue are handled one at a time.
                key_queue::entry_t event;
                if ( key_queue::get(&event) )
                    lua::handle_key_event(event.slot_index, event.is_press);
                // Any remaining key events will be handled next time without sleeping.
                break;

            // Since FLAG_PENDING_CALLS and FLAG_EXECUTE_REPL have lower priority, they
            // are processed only after all higher-priority flags have been handled.
            // While executing these flags, new key events may still occur from
            // matrix_thread, but they will not be processed until the current flag
            // finishes processing. Events can still be sent to usb_thread during this
            // time, but only through explicit calls to fw.send_key().

            case FLAG_PENDING_CALLS:
                if ( usb_thread::is_idle() && matrix_thread::is_idle() )
                    lua::execute_pending_calls();
                break;

            case FLAG_EXECUTE_REPL:
                // Note that FLAG_EXECUTE_REPL is signaled for the first input from
                // stdin, then all remaining inputs are handled within
                // lua::repl::execute().
                if constexpr ( ENABLE_LUA_REPL ) {
                    if ( usb_thread::is_idle() && matrix_thread::is_idle() )
                        lua::repl::execute();
                }
                break;

            case FLAG_TIMEOUT:
                // LOG_DEBUG("Main: v_5v=%d v_con1=%d v_con2=%d fsmstatus=0x%x @%lu\n",
                //     adc::v_5v.read(), adc::v_con1.read(), adc::v_con2.read(),
                //     usb_thread::fsmstatus(),
                //     ztimer_now(ZTIMER_MSEC));
                break;
        }
    }
}
