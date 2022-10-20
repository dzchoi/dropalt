#include "periph/wdt.h"
#include "xtimer.h"

#include "adc_input.hpp"
#include "adc_thread.hpp"
#include "keymap_thread.hpp"
#include "main_thread.hpp"
#include "matrix_thread.hpp"
#include "usb_thread.hpp"



uint32_t info = 0;

int main()
{
    // Create all threads in the order of dependency.
    (void)main_thread::obj();
    (void)adc_thread::obj();
    (void)usb_thread::obj();
    (void)matrix_thread::obj();
    (void)keymap_thread::obj();

    while ( true ) {
        wdt_kick();
        xtimer_sleep(1);

        // char buffer[32];
        // sprintf(buffer, "%lu\n", info);
        // usb_thread::obj().hid_raw.puts(buffer);

        // printf(
        //     "v: %d %d %d fsmstatus=0x%x keyboard_protocol=%u info=%lu\n",
        //     adc_input::v_5v.read(),
        //     adc_input::v_con1.read(),
        //     adc_input::v_con2.read(),
        //     usb_thread::obj().fsmstatus(),
        //     usb_thread::obj().hid_keyboard.get_protocol(),
        //     info);
        // LED0_TOGGLE;
    }

    return 0;
}
