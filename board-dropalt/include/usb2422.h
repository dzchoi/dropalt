#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum {
    // These enum values are deliberately chosen to match their ADC_LINE_ number, and
    // so can be used directly with adc_get(). See the definitions for ADC_LINE_* in
    // board-alt/include/periph_conf.h.
    USB_PORT_UNKNOWN  = 0,  // These correspond to ADC_LINE_5V,
    USB_PORT_1        = 2,  // to ADC_LINE_CON1, and
    USB_PORT_2        = 1   // to ADC_LINE_CON2
};

enum {
    USB_EXTRA_STATE_FORCED_DISABLED = -1,
    USB_EXTRA_STATE_DISABLED        = 0,
    USB_EXTRA_STATE_ENABLED         = 1
};

extern volatile uint8_t usb_extra_port;  // The adc_t ends up with unsigned.
extern volatile int8_t usb_extra_state;
extern volatile uint8_t usb_extra_manual;

void usbhub_init(void);
void usbhub_wait_for_host(uint8_t);

void usbhub_set_extra_state(int8_t state);
void usbhub_handle_extra_device(bool is_extra_plugged_in);

#ifdef __cplusplus
}
#endif
