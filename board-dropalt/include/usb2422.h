#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void usbhub_init(void);

bool usbhub_configure(void);

static inline bool usbhub_active(void) {
    return gpio_read(USB2422_ACTIVE) != 0;
}

enum {
    // These enum values are intentionally chosen to match their corresponding ADC_LINE_*
    // numbers, so they can be used alternatively for adc_get().
    USB_PORT_UNKNOWN  = 0,
    USB_PORT_1        = 1,  // corresponds to ADC_LINE_CON1.
    USB_PORT_2        = 2   // corresponds to ADC_LINE_CON2.
};

static inline uint8_t other(uint8_t port)
{
    if ( port != USB_PORT_UNKNOWN ) {
        if ( port == USB_PORT_1 )
            port = USB_PORT_2;
        else
            port = USB_PORT_1;
    }

    return port;
}

void usbhub_disable_all_ports(void);

void usbhub_enable_host_port(uint8_t port);

uint8_t usbhub_host_port(void);

void usbhub_enable_disable_extra_port(uint8_t port, bool on_off);

#ifdef __cplusplus
}
#endif
