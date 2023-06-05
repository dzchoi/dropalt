#define LOCAL_LOG_LEVEL LOG_INFO

#include "log.h"
#include "periph/wdt.h"         // for wdt_kick()
#include "ztimer.h"             // for ztimer_sleep()

#include "adc_input.hpp"
#include "adc_thread.hpp"
#include "effects.hpp"
#include "keymap_thread.hpp"
#include "main_thread.hpp"
#include "matrix_thread.hpp"
#include "persistent.hpp"
#include "rgb_thread.hpp"
#include "usb_thread.hpp"



unsigned info = 0;

// main() is the thread entry function of main thread. See kernel_init().
int main()
{
    // Setup Seeprom.
    persistent::init();

    // Create all threads in the order of dependency.
    (void)main_thread::obj();
    (void)rgb_thread::obj();
    (void)adc_thread::obj();
    (void)usb_thread::obj();
    (void)keymap_thread::obj();
    (void)matrix_thread::obj();

    if constexpr ( RGB_LED_ENABLE ) {
        // static fixed_color effect = persistent::obj().led_color;
        // static glimmer effect { persistent::obj().led_color, 5 *MS_PER_SEC };
        // static finger_trace effect { persistent::obj().led_color, 5 *MS_PER_SEC };
        static ripple effect { persistent::obj().led_color, 250, 50 };
        rgb_thread::obj().set_effect(effect);
    }

    while ( true ) {
        wdt_kick();

        static bool to_show_previous_logs = persistent::has_log();
        if ( to_show_previous_logs && thread_flags_clear(main_thread::FLAG_USB_RESUME) ) {
            // Use LOG_INFO instead of LOG_ERROR to not add duplicate logs.
            LOG_INFO(persistent::get_log());
            to_show_previous_logs = false;
        }

        // char buffer[32];
        // sprintf(buffer, "%lu\n", info);
        // usb_thread::obj().hid_raw.print(buffer);

        LOG_DEBUG(
            "v_5v=%d v_con1=%d v_con2=%d %s-mode fsmstatus=0x%x info=0x%x\n",
            adc_input::v_5v.read(),
            adc_input::v_con1.read(),
            adc_input::v_con2.read(),
            usb_thread::obj().hid_keyboard.get_protocol() ? "nkro" : "6kro",
            usb_thread::obj().fsmstatus(),
            info);

        ztimer_sleep(ZTIMER_MSEC, 1000);
    }

    return 0;
}
