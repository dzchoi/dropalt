#include <stdio.h>

#include "periph/wdt.h"
#include "xtimer.h"
#include "usb2422.h"

#include "adc_thread.hpp"
#include "main_thread.hpp"
#include "usb_thread.hpp"



int main()
{
    (void)main_thread::instance();  // Create Main thread.

    // Create ADC thread
    adc_thread::instance().wait_for_stable_5v();

    // Todo: These functions will belong to adc_thread.
    usbhub_init();
    usbhub_wait_for_host(USB_PORT_1);

    (void)usb_thread::instance();  // Create USB thread.

    while ( true ) {
        wdt_kick();
        xtimer_sleep(1);

        printf("v: %d\n", adc_thread::instance().v_5v());
        // usb_thread::instance().console_printf(...);
    }

    return 0;
}
