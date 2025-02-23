// This module initializes the USB2422 hub and manages the host and extra USB ports
// connected to it. The functions in this module should be called from a thread context,
// as they depend on ztimer.

#pragma once

#include "periph/gpio.h"
#include "sr_exp.h"             // for sr_exp_data



#ifdef __cplusplus
extern "C" {
#endif

void usbhub_init(void);

bool usbhub_configure(void);

// Note that signal_usb_resume() (i.e. usb active) could be called by an interrupt
// when detecting GPIO_RISING on the USB2422_ACTIVE pin (= GPIO_PIN(PA, 18)) instead.
// However, Riot's gpio_init_int() seems assigning the same _exti (i.e. EXTINTx) to
// the gpio pin as with row_pins[2] (= GPIO_PIN(PA, 2)), causing one of them not
// working. The signal_usb_resume() is instead triggered by usb_thread (on
// USBUS_EVENT_USB_RESUME). See usbus_hid_keyboard_t::on_resume().
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

// Set the host port to the specified port.
void usbhub_select_host_port(uint8_t port);

// Enable or disable VBUS for the specified extra port.
void usbhub_enable_extra_port(uint8_t port, bool enable);

// Retrieve the host port based on the value from the most recent call to
// sr_exp_writedata().
static inline uint8_t usbhub_host_port(void) {
    return sr_exp_data.bit.E_UP_N ? USB_PORT_UNKNOWN :
        (sr_exp_data.bit.S_UP ? USB_PORT_2 : USB_PORT_1);
}

// Retrieve the extra port based on the value from the most recent call to
// sr_exp_writedata(), even if the port is not connected to a device.
// (Internally, it reads from S_UP instead of S_DN1, which is available only when
// usbhub_switch_enable_extra_port() is called.)
static inline uint8_t usbhub_extra_port(void) {
    return sr_exp_data.bit.E_UP_N ? USB_PORT_UNKNOWN :
        (sr_exp_data.bit.S_UP ? USB_PORT_1 : USB_PORT_2);
}

// Check if the port's CC is set up for a host instead of a device.
static inline bool usbhub_is_configured_for_host_port(uint8_t port) {
    if ( port == USB_PORT_UNKNOWN )
        return false;
    return port == USB_PORT_1 ? sr_exp_data.bit.SRC_1 : sr_exp_data.bit.SRC_2;
}

#ifdef __cplusplus
}
#endif
