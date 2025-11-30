#include "assert.h"
#include "auto_init.h"          // for auto_init()
#include "board.h"              // for _sheap, _eheap, system_reset()
#include "cpu.h"                // for RSTC
#include "irq.h"                // for irq_disable(), irq_restore()
#include "led_conf.h"           // for KEY_LED_COUNT
#include "log.h"                // for set_log_mask()
#include "periph/wdt.h"         // for wdt_kick()
#include "thread.h"             // for thread_get_active(), cpu_switch_context_exit()
#include "thread_flags.h"       // for thread_flags_set(), thread_flags_wait_one()
#include "ztimer.h"             // for ztimer_set_timeout_flag()

#include "adc.hpp"              // for adc::init()
#include "event_ext.hpp"        // for event_post(), event_queue_init(), event_get()
#include "lua.hpp"              // for lua::global_lua_state::init(), ...
#include "config.hpp"           // for ENABLE_CDC_ACM, ENABLE_LUA_REPL
#include "main_key_events.hpp"  // for main_key_events::push(), ...
#include "lexecute.hpp"         // for lua::execute_pending_calls(), ...
#include "lkeymap.hpp"          // for lua::handle_key_event(), lua::handle_lamp_state()
#include "main_thread.hpp"
#include "matrix_thread.hpp"    // for matrix_thread::init(), matrix_thread::is_idle()
#include "persistent.hpp"       // for persistent::init()
#include "repl.hpp"             // for lua::repl::init(), ...
#include "rgb_gcr.hpp"          // for rgb_gcr::enable(), rgb_gcr::disable()
#include "timed_stdin.hpp"      // for timed_stdin::wait_for_input(), ...
#include "usb_thread.hpp"       // for usb_thread::init(), usb_thread::is_idle()
#include "usbhub_thread.hpp"    // for usbhub_thread::init()



// This kernel_init() function is called from reset_handler_default() in
// riot/cpu/cortexm_common/vectors_cortexm.c.
extern "C" NORETURN void kernel_init()
{
    irq_disable();

    // Set up the first thread, main_thread.
    main_thread::init();

    // Switch from the boot code context to main_thread.
    cpu_switch_context_exit();
}



thread_t* main_thread::m_pthread;

alignas(8) char main_thread::m_thread_stack[THREAD_STACKSIZE_MAIN];

event_queue_t main_thread::m_event_queue;

ztimer_t main_thread::m_heartbeat_timer;

void* (*main_thread::m_active_mode)(void*);

void main_thread::init()
{
    LOG_INFO("Main: This is RIOT! (Version: " RIOT_VERSION ")");

    m_pthread = thread_get_unchecked( thread_create(
        m_thread_stack, sizeof(m_thread_stack),
        THREAD_PRIORITY_MAIN,
        THREAD_CREATE_WOUT_YIELD,
        _thread_entry, nullptr, "main_thread") );
}

// Wake up from wait_for_input() without setting any flags, allowing subsequent checks
// of usb_thread::is_idle() and matrix_thread::is_idle().
void main_thread::signal_thread_idle()
{
    if constexpr ( ENABLE_LUA_REPL )
        timed_stdin::stop_wait();
}

void main_thread::set_thread_flags(thread_flags_t flags)
{
    thread_flags_set(m_pthread, flags);
    if constexpr ( ENABLE_LUA_REPL )
        timed_stdin::stop_wait();
}

void main_thread::set_my_flags(thread_flags_t flags)
{
    // assert( is_active() );  // Ensure that it is called from main_thread.
    unsigned state = irq_disable();
    m_pthread->flags |= flags;
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
    LOG_DEBUG("Matrix: %s [%u] @%lu",
        press_or_release(is_press), slot_index, ztimer_now(ZTIMER_MSEC));

    if ( likely(main_key_events::push({{ uint8_t(slot_index), is_press }}, timeout_us)) )
    {
        set_thread_flags(FLAG_KEY_EVENT);
        return true;
    }

    LOG_ERROR("Main: main_key_events::push() failed");

    // If the key event queue is full with all deferred events, it means a deadlock.
    // Trigger a system reset then.
    if ( unlikely(main_key_events::terminal_full()) )
        system_reset();

    // Otherwise, discard the unaffordable event. The matrix_thread will report it again
    // later, as long as the key remains pressed.
    return false;
}

void main_thread::signal_lamp_state(uint8_t lamp_state)
{
    static event_ext_t<uint8_t> _event = { nullptr,
        [](event_t* event) {
            lua::handle_lamp_state(static_cast<event_ext_t<uint8_t>*>(event)->arg);
        },
        0
    };

    _event.arg = lamp_state;
    signal_event(&_event);
}

NORETURN void* main_thread::_thread_entry(void*)
{
    // Now running in thread context, and since we won't return to the boot code (
    // main_thread was created using THREAD_CREATE_WOUT_YIELD and entered via
    // cpu_switch_context_exit()), we reset the ISR stack pointer (MSP). From this point
    // on, MSP will be used exclusively for handling interrupts and faults.
    __asm__ volatile (
        "cpsid i\n"
        "msr msp, %[estack]\n"
        "cpsie i\n"
        :
        : [estack] "r" (&_estack)
    );

    bool forced_dfu = has_bootmagic_number();

    // Determine m_active_mode before calling usb_thread::init().
    m_active_mode =
        // Stay in DFU mode if bootmagic number is set.
        !forced_dfu &&
        // Stay in DFU mode if the reset cause is not system or power-on; in case of
        // RSTC_RCAUSE_NVM, we stay in DFU mode because the opposite bank does not have
        // a valid Lua bytecode yet.
        (RSTC->RCAUSE.reg & (RSTC_RCAUSE_SYST | RSTC_RCAUSE_POR)) != 0 &&
        // Also stay in DFU mode if no valid Lua bytecode is found.
        lua::global_lua_state::validate_bytecode(SLOT0_OFFSET + RIOTBOOT_HDR_LEN)
        ? &normal_mode : &dfu_mode;

    if ( IS_USED(MODULE_AUTO_INIT) )
        auto_init();     // ztimer_init(), ...

    // Initialize subsystems in the order of dependency.
    adc::init();         // Invokes v_5v.wait_for_stable_5v().
    persistent::init();  // Initialize NVM. (See `last_host_port` for a usage example.)
    usbhub_thread::init();
    usb_thread::init();  // printf() will work from this point, displaying on the host.
    matrix_thread::init();  // Produces signals to main_thread.

    // The event_queue_init() should be called from the queue-owning thread.
    event_queue_init(&m_event_queue);

    if ( forced_dfu )
        LOG_DEBUG("Main: Forced DFU mode");

    LOG_DEBUG("Main: max heap size is %d bytes", &_eheap - &_sheap);

    while ( true )
        m_active_mode = (void* (*)(void*))m_active_mode(nullptr);
}

// 🚨 Note: This function acts as the bootloader. Modify DFU mode code with caution, as
// DFU mode serves as a failsafe for handling hard faults and flashing both firmware and
// Lua bytecode.
void* main_thread::dfu_mode(void*)
{
    LOG_INFO("Main: Welcome to DFU mode");

    bool exit = false;
    while ( !exit ) {
#ifdef DEVELHELP
        // Refresh the watchdog timer to keep the system alive. If we don't come back
        // here within the watchdog timeout, the system will trigger a watchdog reset.
        wdt_kick();
#endif

        ztimer_set_timeout_flag(ZTIMER_MSEC, &m_heartbeat_timer, HEARTBEAT_PERIOD_MS);
        thread_flags_t flag = thread_flags_wait_one(
            FLAG_KEY_EVENT | FLAG_MODE_TOGGLE | FLAG_TIMEOUT);  // Zzz

        switch ( flag ) {
            // Note that FLAG_DTE_* signals are generally unexpected in DFU mode but
            // passed to normal mode if present. If FLAG_DTE_ENABLED occurs, it is
            // typically followed by FLAG_DTE_READY and then FLAG_DTE_DISABLED (~500 ms).
            // In that case, normal mode will ultimately treat the sequence as
            // FLAG_DTE_DISABLED, ignoring FLAG_DTE_READY.

            case FLAG_KEY_EVENT:
                main_key_events::key_event_t event;
                // If the ESC key is released, trigger system_reset(). All other keys are
                // ignored.
                // Note that seeprom_bkswrst() might be more appropriate if DFU mode was
                // entered after DFU_DNLOAD completes (without `dfu-util -R`). In this
                // case, `exit = true` would cause an assert failure in normal mode,
                // since Lua bytecode isn't available on the opposite bank.
                // If DFU mode was entered during power-up, `exit = true` would be good,
                // but system_reset() works in both scenarios.
                // if ( main_key_events::get(&event) && event.slot_index == 67 )
                if ( main_key_events::get(&event) && event.slot_index == 1  // ESC
                  && !event.is_press )
                    system_reset();
                break;

            case FLAG_MODE_TOGGLE:
                exit = true;
                break;
        }
    }

    LED0_OFF;
    if constexpr ( ENABLE_RGB_LED )
        rgb_gcr::enable();

    return (void*)&normal_mode;
}

void* main_thread::normal_mode(void*)
{
    LOG_INFO("Main: Welcome to Normal mode");

    // Simple tests to trigger a hard fault. Will reboot to DFU mode.
    // __attribute__((unused)) volatile int p = *(int*)(0);  // MemManage fault
    // void (*f)() = (void (*)())0x08000000; f();  // HardFault (executing even address)
    // __builtin_trap();  // HardFault
    // assert( false );

    // Set up the global Lua state and load the keymap module, initializing all keymaps
    // and LED effects.
    lua::global_lua_state::init();

    bool exit = false;
    while ( true ) {
#ifdef DEVELHELP
        wdt_kick();
#endif

        // No flags. Time for some housekeeping and a nap.
        if ( (m_pthread->flags & ALL_FLAGS) == 0 ) {
            // Pending calls and REPL are processed only when no flags are set. During
            // this time, key events from matrix_thread may still occur, but they won't
            // be handled until processing completes. Events can still be sent to
            // usb_thread, but only through explicit calls to fw.send_key().
            if ( usb_thread::is_idle() && matrix_thread::is_idle() ) {
                if ( lua::execute_is_pending() ) {
                    lua::execute_pending_calls();
                    continue;
                }

                if constexpr ( ENABLE_LUA_REPL ) {
                    if ( timed_stdin::has_input() ) {
                        // `repl::execute()` starts with the initial chunk received from
                        // wait_for_input(), then continues reading the remaining parts
                        // internally, uninterrupted by stop_read().
                        lua::repl::execute();
                        continue;
                    }
                }

                // Leave normal mode if prompted.
                if ( exit )
                    break;
            }

            // `wait_for_input()` is called when either not both threads are idle or
            // input is unavailable. If any flag is set during the wait, it returns
            // immediately.
            if constexpr ( ENABLE_LUA_REPL ) {
                if ( !timed_stdin::has_input() ) {
                    timed_stdin::wait_for_input(HEARTBEAT_PERIOD_MS, ALL_FLAGS);  // Zzz
                    continue;
                }
            }

            // If input is available (not processed though), wait_for_input() is not
            // called again. Instead, we set a wake-up timer and enter sleep via
            // thread_flags_wait_one() below.
            ztimer_set_timeout_flag(
                ZTIMER_MSEC, &m_heartbeat_timer, HEARTBEAT_PERIOD_MS);
        }

        // Wait for a signal within HEARTBEAT_PERIOD_MS if no flags are set.
        // If any flags are already set (as is always the case for ENABLE_LUA_REPL),
        // thread_flags_wait_one() returns immediately, starting with the LSB, without
        // sleeping.
        thread_flags_t flag = thread_flags_wait_one(ALL_FLAGS);  // Zzz

        switch ( flag ) {
            case FLAG_GENERIC_EVENT:
                // Key timeout and lamp events are prioritized and processed first via
                // m_event_queue.
                while ( event_t* event = event_get(&m_event_queue) )
                    event->handler(event);
                break;

            case FLAG_DTE_ENABLED:
                if constexpr ( ENABLE_CDC_ACM )
                    LOG_DEBUG("Main: DTE enabled");
                break;

            case FLAG_DTE_DISABLED:
                if constexpr ( ENABLE_LUA_REPL ) {
                    if ( timed_stdin::is_enabled() ) {
                        lua::repl::stop();
                        timed_stdin::disable();
                        LOG_INFO("Main: REPL disabled");
                    }
                }
                if constexpr ( ENABLE_CDC_ACM )
                    LOG_DEBUG("Main: DTE disabled");

                // If FLAG_DTE_DISABLED comes along with FLAG_DTE_READY, FLAG_DTE_READY
                // is treated as stale and ignored. See the related comment in
                // dfu_mode().
                thread_flags_clear(FLAG_DTE_READY);
                break;

            case FLAG_DTE_READY:
                if constexpr ( ENABLE_LUA_REPL ) {
                    if ( !timed_stdin::is_enabled() ) {
                        timed_stdin::enable();
                        lua::repl::start();
                        LOG_INFO("Main: REPL ready (0x%x)", get_log_mask());
                    }
                }
                // Do not process FLAG_DTE_READY when ENABLE_LUA_REPL is false. The
                // stdin remains permanently disabled.
                break;

            case FLAG_KEY_EVENT:
                // Key (press/release) events from main_key_events are handled one at a
                // time.
                main_key_events::key_event_t event;
                if ( main_key_events::get(&event) ) {
                    lua::handle_key_event(event.slot_index, event.is_press);
                    // Any remaining key events will be handled next time without
                    // sleeping.
                    set_my_flags(FLAG_KEY_EVENT);
                }
                break;

            case FLAG_MODE_TOGGLE:
                exit = true;
                break;

            case FLAG_TIMEOUT:
                // LOG_DEBUG("Main: v_5v=%d v_con1=%d v_con2=%d fsmstatus=0x%x @%lu",
                //     adc::v_5v.read(), adc::v_con1.read(), adc::v_con2.read(),
                //     usb_thread::fsmstatus(),
                //     ztimer_now(ZTIMER_MSEC));
                break;
        }
    }

    lua::global_lua_state::destroy();

    if constexpr ( ENABLE_RGB_LED )
        rgb_gcr::disable();
    LED0_ON;

    return (void*)&dfu_mode;
}
