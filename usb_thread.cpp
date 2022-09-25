#include <cstdarg>              // for va_list
#include <cstdio>               // for vsnprintf

#include "usb_thread.hpp"

// #include "_keymap_config.h"



/*
static uint8_t _keyboard_led_state;

uint8_t keyboard_leds(void)
{
    return _keyboard_led_state;
}

// Todo: Instead of polling for LED state, trigger an event to md_rgb_matrix_indicators.
static void _set_keyboard_leds(usbus_hid_device_t* hid, uint8_t* data, size_t len)
{
    (void)hid;
    if ( len == 2 ) {
        const uint8_t report_id = data[0];
        if ( report_id == REPORT_ID_KEYBOARD || report_id == REPORT_ID_NKRO )
            _keyboard_led_state = data[1];
    } else
        _keyboard_led_state = data[0];
}
*/



// Relationship between hid_boot and hid_shared:
//
//                      | KEYBOARD_SHARED_EP == 0 | KEYBOARD_SHARED_EP == 1
// ---------------------+-------------------------+-------------------------
// SHARED_EP_ENABLE = 0 | hid_boot, no hid_shared | N/A
// ---------------------+-------------------------+-------------------------
// SHARED_EP_ENABLE = 1 | hid_boot != hid_shared  | hid_boot == hid_shared
//

extern "C" void usb_cdc_acm_stdio_init(usbus_t* usbus);

usb_thread::usb_thread()
{
    usbdev_t* usbdev = usbdev_get_ctx(0);
    usbus_init(&m_usbus, usbdev);

    // Todo: cb = _set_keyboard_leds
    hid_boot.pre_init(&m_usbus, nullptr);

    // Todo: Make sure that the Interface number is 1 (the 2nd Interface?)
    hid_raw.pre_init(&m_usbus);

    hid_mouse.pre_init(&m_usbus);

    // Todo: cb = _set_keyboard_leds
    hid_shared.pre_init(&m_usbus, nullptr);

    hid_console.pre_init(&m_usbus);
    // usbus_hid_init(&m_usbus, hid_con, _usbus_hid_cb, CtapReport, CtapReportSize);

#ifdef VIRTSER_ENABLE
    // USB serial console (USB CDC-ACM device) connected on /dev/ttyACMx on Linux
    usb_cdc_acm_stdio_init(&m_usbus);
#endif

    // Create "usbus" thread.
    usbus_create(m_stack, USBUS_STACKSIZE, THREAD_PRIO_USB, USBUS_TNAME, &m_usbus);
    m_pthread = thread_get(m_usbus.pid);
}

// Can possibly miss to send the report depending on the current usbus state (e.g.
// USBUS_STATE_SUSPEND), which could be checked by checking the result of
// usbus_hid_submit(), but is not. See usbus_hid_submit() for the detail.
void usb_thread::async_send_keyboard(report_keyboard_t* report)
{
#ifdef NKRO_ENABLE
    if ( keymap_config.nkro && keyboard_protocol )  // NKRO Protocol
        hid_keyboard.submit((const uint8_t*)report, sizeof(struct nkro_report));
    else
#endif
    if ( keyboard_protocol )
        // Report Protocol
        hid_keyboard.submit((const uint8_t*)report, KEYBOARD_REPORT_SIZE);
    else
        // Boot Protocol
        hid_keyboard.submit((const uint8_t*)&report, KEYBOARD_EPSIZE);
            // Todo: Implement report->mods.
            // (const uint8_t*)&report->mods, KEYBOARD_EPSIZE);
}

#ifdef EXTRAKEY_ENABLE
static void _send_extrakey(uint8_t report_id, uint16_t data)
{
    report_extra_t report = { .report_id = report_id, .usage = data };
    hid_shared.submit((const uint8_t*)&report, sizeof(report_extra_t));
}
#endif

void usb_thread::async_send_system(uint16_t data)
{
#ifdef EXTRAKEY_ENABLE
    _send_extrakey(REPORT_ID_SYSTEM, data);
#else
    (void)data;
#endif
}

void usb_thread::async_send_consumer(uint16_t data)
{
#ifdef EXTRAKEY_ENABLE
    _send_extrakey(REPORT_ID_CONSUMER, data);
#else
    (void)data;
#endif
}

void usb_thread::async_send_mouse(report_mouse_t* report)
{
#ifdef MOUSE_ENABLE
#   ifdef MOUSE_SHARED_EP
    hid_shared.submit((const uint8_t*)report, sizeof(report_mouse_t));
#   else
    hid_mouse.submit((const uint8_t*)report, sizeof(report_mouse_t));
#   endif
#else
    (void)report;
#endif
}

int usb_thread::console_send(const void* buf, size_t len)
{
#ifdef CONSOLE_ENABLE
    size_t offset = 0;
    while ( offset < len ) {
        const size_t written = hid_console.submit(
            (const uint8_t*)buf + offset, len - offset);
        if ( written == 0 )
            return -1;  // indicate an error (< 0)
        offset += written;
    }

    return (int)offset;
#else
    (void)buf;
    (void)len;
    return -1;
#endif
}

int usb_thread::console_printf(const char* fmt, ...)
{
#ifdef CONSOLE_ENABLE
    static char _buf[CONSOLE_PRINTBUF_SIZE];

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(_buf, CONSOLE_PRINTBUF_SIZE, fmt, args);
    va_end(args);

    if ( written > 0 )
        written = console_send(_buf, written);
    return written;
#else
    (void)fmt;
    return -1;
#endif
}

// Required to be called only when m_usbus.state == USBUS_STATE_SUSPEND.
void usb_thread::async_send_remote_wake_up(void)
{
    // "First, the USB must have detected a “Suspend” state on the bus, i.e. the remote
    // wakeup request can only be sent after INTFLAG.SUSPEND has been set.
    // The user may then write a one to the Remote Wakeup bit (CTRLB.UPRSM) to send an
    // Upstream Resume to the host initiating the wakeup. This will automatically be
    // done by the controller after 5 ms of inactivity on the USB bus.
    // When the controller sends the Upstream Resume INTFLAG.WAKEUP is set and INTFLAG.
    // SUSPEND is cleared.
    // The CTRLB.UPRSM is cleared at the end of the transmitting Upstream Resume."

    UsbDevice* const device = ((sam0_common_usb_t*)m_usbus.dev)->config->device;

    if ( (device->CTRLB.reg & USB_DEVICE_CTRLB_UPRSM) == 0 )
        device->CTRLB.reg |= USB_DEVICE_CTRLB_UPRSM;
}
