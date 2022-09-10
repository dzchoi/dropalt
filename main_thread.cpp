#include <stdio.h>

#include "periph/wdt.h"
#include "xtimer.h"

#include "main_thread.hpp"
#include "usb_thread.hpp"
#include "usb2422.h"



int main()
{
    (void)main_thread::instance();  // Create Main thread.

    // Todo: These functions will belong to adc_thread.
    usbhub_init();
    usbhub_wait_for_host(USB_PORT_1);

    (void)usb_thread::instance();  // Create USB thread.

    int i = 0;
    while ( true ) {
        wdt_kick();
        xtimer_sleep(1);

        usb_thread::instance().console_printf("alive %d\n", ++i);
        // printf("alive %d\n", i);
    }

    return 0;
}
