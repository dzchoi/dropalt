#pragma once

#include "periph/gpio.h"
#include "sr_exp.h"             // for sr_exp_data

#ifdef __cplusplus
extern "C" {
#endif

void usbhub_init(gpio_cb_t cb, void* arg);

bool usbhub_configure(void);

static inline bool usbhub_is_active(void) {
    return gpio_read(USB2422_ACTIVE) != 0;
}

enum {
    // These enum values are intentionally chosen to match their corresponding ADC_LINE_*
    // numbers, so they can be used for adc_get().
    USB_PORT_UNKNOWN  = 0,
    USB_PORT_1        = 1,  // corresponds to ADC_LINE_CON1.
    USB_PORT_2        = 2   // corresponds to ADC_LINE_CON2.
};

void usbhub_disable_all_ports(void);

void usbhub_enable_host_port(uint8_t port);

// Read the host port from the value from the last call of sr_exp_writedata().
static inline uint8_t usbhub_host_port(void) {
    return sr_exp_data.bit.E_UP_N ? USB_PORT_UNKNOWN :
        (sr_exp_data.bit.S_UP ? USB_PORT_2 : USB_PORT_1);
}

static inline uint8_t usbhub_extra_port(void) {
    return sr_exp_data.bit.E_UP_N ? USB_PORT_UNKNOWN :
        (sr_exp_data.bit.S_UP ? USB_PORT_1 : USB_PORT_2);
}

// VBUS gets connected/cut off to the specified extra USB port when enabled/disabled.
void usbhub_enable_extra_port(uint8_t port, bool yes_no);

#ifdef __cplusplus
}
#endif
