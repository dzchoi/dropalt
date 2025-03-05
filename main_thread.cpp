#include "assert.h"
#include "board.h"              // for _sheap, _eheap
#include "cpu.h"                // for RSTC
#include "log.h"
#include "periph/wdt.h"         // for wdt_kick()
#include "thread.h"             // for thread_get_active()
#include "thread_flags.h"       // for thread_flags_set(), thread_flags_wait_any()
#include "ztimer.h"             // for ztimer_set_timeout_flag()

// Temporary code for testing only CDC ACM.
#include "usb2422.h"            // for usbhub_is_active()

#include "adc.hpp"              // for adc::v_5v, v_con1, v_con2
#include "features.hpp"         // for ENABLE_CDC_ACM
#include "main_thread.hpp"
#include "usb_thread.hpp"       // for usb::init()
#include "usbhub_thread.hpp"    // for usbhub::init()



// This main() function is called from main_trampoline() in the "main" thread context.
// See kernel_init() for more information.
extern "C" int main()
{
    main::init();
}



// "class" is necessary here to distinguish the "main" class from main() function.
class main main::m_instance;

NORETURN void main::init()
{
    m_instance.m_pthread = thread_get_active();

    // Create all threads in the order of dependency.
    usb::init();        // printf() will now start to work, displaying on the host.
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

    LOG_DEBUG("Main: total heap size is %d bytes\n", &_eheap - &_sheap);

    // Temporary code for testing only CDC ACM.
    while ( !usbhub_is_active() ) { ztimer_sleep(ZTIMER_MSEC, 10); }
    usbhub::thread().signal_usb_resume();

    // m_show_previous_logs = persistent::has_log();

    // Instead of creating a new thread, use the already existing "main" thread.
    (void)_thread_entry(&m_instance);
}

void main::set_thread_flags(thread_flags_t flags)
{
    thread_flags_set(m_pthread, flags);
    // if constexpr ( LUA_REPL_ENABLE )
    //     stdio_stop_read();
}

void main::signal_dte_state(bool dte_enabled)
{
    if constexpr ( ENABLE_CDC_ACM )
        set_thread_flags(dte_enabled ? FLAG_DTE_ENABLED : FLAG_DTE_DISABLED);
    else
        assert( false );
}

NORETURN void* main::_thread_entry(void* arg)
{
    main* const that = static_cast<main*>(arg);
    (void)that;

    thread_flags_t flags;
    while ( true ) {
        // Kick the watchdog timer to keep the system alive. If we don't come back here
        // within the watchdog timeout, the system will trigger a watchdog reset.
        wdt_kick();

        // Set up wake-up time before going to sleep.
        ztimer_t heartbeat;
        ztimer_set_timeout_flag(ZTIMER_MSEC, &heartbeat, HEARTBEAT_PERIOD_MS);

        flags = thread_flags_wait_any(  // Zzz
            FLAG_USB_RESET
            | FLAG_USB_RESUME
            | FLAG_TIMEOUT );

        // if ( m_show_previous_logs && (flags & FLAG_USB_RESUME) ) {
        //     // Use LOG_INFO instead of LOG_ERROR to not add duplicate logs.
        //     LOG_INFO(persistent::get_log());
        //     m_show_previous_logs = false;
        // }

        if ( flags & FLAG_TIMEOUT ) {
            LOG_DEBUG("Main: v_5v=%d v_con1=%d v_con2=%d @%ld\n",
                adc::v_5v.read(), adc::v_con1.read(), adc::v_con2.read(),
                ztimer_now(ZTIMER_MSEC));
        }
    }
}
