#include <stdio.h>

#include "periph/wdt.h"
#include "xtimer.h"

#include "adc_input.hpp"
#include "adc_thread.hpp"
#include "main_thread.hpp"
#include "usb_thread.hpp"



int main()
{
    (void)main_thread::instance();  // Create Main thread.

    // Create ADC thread.
    adc_thread::instance().wait_for_stable_5v();
    adc_thread::instance().async_select_host_port();

    (void)usb_thread::instance();  // Create USB thread.

    while ( true ) {
        wdt_kick();
        xtimer_sleep(1);

        printf("v: %d %d %d (%d %d)\n",
            v_5v.read(),
            v_con1.read(),
            v_con2.read(),
            v_con1.m_result0,
            v_con2.m_result0);
        // usb_thread::instance().console_printf(...);
    }

    return 0;
}
