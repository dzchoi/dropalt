#include "periph/wdt.h"
#include "usb2422.h"
#include "xtimer.h"

#include "adc_input.hpp"
#include "adc_thread.hpp"
#include "main_thread.hpp"
#include "usb_thread.hpp"



int main()
{
    (void)main_thread::obj();  // Create Main thread.
    (void)adc_thread::obj();   // Create ADC thread.
    (void)usb_thread::obj();   // Create USB thread.

    while ( true ) {
        wdt_kick();
        xtimer_sleep(1);

        // usb_thread::obj().console_printf(
        printf(
            "v: %d %d %d hub=%d usb=%d\n",
            adc_input::v_5v.read(),
            adc_input::v_con1.read(),
            adc_input::v_con2.read(),
            usbhub_is_active(),
            usb_thread::obj().fsmstatus());
        // LED0_TOGGLE;
    }

    return 0;
}
