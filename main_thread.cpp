#include "assert.h"
#include "board.h"              // for _sheap, _eheap
#include "cpu.h"                // for RSTC
#include "log.h"                // for set_log_mask()
#include "periph/wdt.h"         // for wdt_kick()
#include "thread.h"             // for thread_get_active()
#include "thread_flags.h"       // for thread_flags_set(), thread_flags_wait_any()
#include "ztimer.h"             // for ztimer_set_timeout_flag()

// Temporary code for testing only CDC ACM.
#include "usb2422.h"            // for usbhub_is_active()

#include "adc.hpp"              // for adc::v_5v, v_con1, v_con2
#include "lua.hpp"              // for lua::init()
#include "config.hpp"           // for ENABLE_CDC_ACM, ENABLE_LUA_REPL
#include "main_thread.hpp"
#include "persistent.hpp"       // for persistent::init()
#include "repl.hpp"             // for lua::repl::init(), ...
#include "timed_stdin.hpp"      // for timed_stdin::wait_for_input(), ...
#include "usb_thread.hpp"       // for usb::init()
#include "usbhub_thread.hpp"    // for usbhub::init()



// This main() function is called from main_trampoline() in the "main" thread context.
// See kernel_init() for more information.
extern "C" int main()
{
    main_thread::init();
}



thread_t* main_thread::m_pthread;

NORETURN void main_thread::init()
{
    m_pthread = thread_get_active();

    // Initialize NVM. (See `last_host_port` for an example of usage.)
    persistent::init();

    // Create all threads in the order of dependency.
    usb::init();        // printf() will work from this point, displaying on the host.
    // rgb::init();
    usbhub::init();
    // keymap::init();
    // matrix::init();

    // RGB_LED related code should be in rgb::init().

    // Refer to riot/cpu/sam0_common/include/vendor/samd51/include/component/rstc.h
    // for the meaning of each bit. For instance, 0x40 indicates a system reset.
    // Resets other than a system reset or power reset are not expected, as they are
    // caught by the bootloader.
    LOG_DEBUG("Main: RSTC->RCAUSE.reg=0x%x\n", RSTC->RCAUSE.reg);

    LOG_DEBUG("Main: max heap size is %d bytes\n", &_eheap - &_sheap);

    // Temporary code for testing only CDC ACM.
    while ( !usbhub_is_active() ) { ztimer_sleep(ZTIMER_MSEC, 10); }
    usbhub::thread().signal_usb_resume();

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

NORETURN void* main_thread::_thread_entry(void*)
{
    // Lua keymap script and REPL will operate on the common lua State.
    lua_State* L = lua::init();

    thread_flags_t flags;
    while ( true ) {
        // Kick the watchdog timer to keep the system alive. If we don't come back here
        // within the watchdog timeout, the system will trigger a watchdog reset.
        wdt_kick();

        if constexpr ( ENABLE_LUA_REPL ) {
            bool has_input = timed_stdin::wait_for_input(HEARTBEAT_PERIOD_MS);  // Zzz

            flags = thread_flags_clear(
                FLAG_USB_RESET
                | FLAG_USB_RESUME
                | FLAG_DTE_DISABLED
                | FLAG_DTE_ENABLED
                | FLAG_DTE_READY
                | FLAG_TIMEOUT );

            if ( flags & FLAG_DTE_ENABLED )
                LOG_INFO("Main: DTE enabled\n");

            if ( flags & FLAG_DTE_DISABLED ) {
                timed_stdin::disable();
                lua::repl::stop(L);
                LOG_INFO("Main: DTE disabled\n");
            }

            if ( flags & FLAG_DTE_READY ) {
                lua::repl::init(L);
                timed_stdin::enable();
                LOG_DEBUG("Main: DTE ready (0x%x)\n", get_log_mask());
            }

            // Process key events here before executing REPL.

            if ( has_input )
                lua::repl::execute(L);
        }

        else {
            // Set up wake-up time before going to sleep.
            ztimer_t heartbeat;
            ztimer_set_timeout_flag(ZTIMER_MSEC, &heartbeat, HEARTBEAT_PERIOD_MS);

            flags = thread_flags_wait_any(  // Zzz
                FLAG_USB_RESET
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

        if ( flags & FLAG_TIMEOUT ) {
            LOG_DEBUG("Main: v_5v=%d v_con1=%d v_con2=%d @%ld\n",
                adc::v_5v.read(), adc::v_con1.read(), adc::v_con2.read(),
                ztimer_now(ZTIMER_MSEC));
        }
    }
}
