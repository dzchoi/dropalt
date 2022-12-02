#include "periph/wdt.h"
#include "xtimer.h"

#define ENABLE_DEBUG    0
#include "debug.h"

#include "adc_input.hpp"
#include "adc_thread.hpp"
#include "effects.hpp"
#include "keymap_thread.hpp"
#include "main_thread.hpp"
#include "matrix_thread.hpp"
#include "rgb_thread.hpp"
#include "usb_thread.hpp"



unsigned info = 0;

int main()
{
    // Create all threads in the order of dependency.
    (void)main_thread::obj();
    (void)adc_thread::obj();
    (void)rgb_thread::obj();
    (void)usb_thread::obj();
    (void)matrix_thread::obj();
    (void)keymap_thread::obj();

    if constexpr ( RGB_LED_ENABLE ) {
        // https://stackoverflow.com/questions/21737613/image-of-hsv-color-wheel-for-opencv
        constexpr uint16_t orange = 30 * hsv_t::COLORS / 360;
        // constexpr uint16_t spring_green = 90 * hsv_t::COLORS / 360;

        // fixed_color effect = hsv_t{ orange, 255, 255 };
        // glimmer effect { hsv_t{ orange, 255, 255 }, 5 *MS_PER_SEC };
        // finger_trace effect { hsv_t{ orange, 255, 255 }, 5 *MS_PER_SEC };
        ripple effect { hsv_t{ orange, 255, 255 }, 250, 50 };

        // rgb_thread::set_color_all(rgb_t{0, 255, 128});
        rgb_thread::obj().set_effect(effect);
    }

    while ( true ) {
        wdt_kick();
        xtimer_sleep(1);

        // char buffer[32];
        // sprintf(buffer, "%lu\n", info);
        // usb_thread::obj().hid_raw.print(buffer);

        DEBUG(
            "v_5v=%d v_con1=%d v_con2=%d %s-mode fsmstatus=0x%x info=0x%x\n",
            adc_input::v_5v.read(),
            adc_input::v_con1.read(),
            adc_input::v_con2.read(),
            usb_thread::obj().hid_keyboard.get_protocol() ? "nkro" : "6kro",
            usb_thread::obj().fsmstatus(),
            info);
    }

    return 0;
}
