#include "features.hpp"         // for RAW_REPORT_INTERVAL_MS
#include "usbus_hid_raw.hpp"



void usbus_hid_raw_tl<true>::init(usbus_t* usbus)
{
    // Configure generic HID device interface
    iface._class = USB_CLASS_HID;
    iface.subclass = USB_HID_SUBCLASS_NONE;
    iface.protocol = USB_HID_PROTOCOL_NONE;
    iface.descr_gen = &hid_descr;
    iface.handler = &handler_ctrl;

    // IN endpoint to send data to host
    ep_in = usbus_add_endpoint(usbus, &iface,
                               USB_EP_TYPE_INTERRUPT,
                               USB_EP_DIR_IN,
                               RAW_EPSIZE);
    ep_in->interval = RAW_REPORT_INTERVAL_MS;  // interrupt endpoint polling rate
    usbus_enable_endpoint(ep_in);

    // OUT endpoint to receive data from host
    ep_out = usbus_add_endpoint(usbus, &iface,
                                USB_EP_TYPE_INTERRUPT,
                                USB_EP_DIR_OUT,
                                RAW_EPSIZE);
    ep_out->interval = RAW_REPORT_INTERVAL_MS;
    usbus_enable_endpoint(ep_out);

    // Signal that INTERRUPT OUT is ready to receive data the host.
    usbdev_ep_xmit(ep_out->ep, out_buf, 0);

    usbus_add_interface(usbus, &iface);
}

int usbus_hid_raw_tl<true>::puts(const char* str)
{
    size_t offset = 0;
    size_t len = __builtin_strlen(str);
    while ( offset < len ) {
        const size_t written = submit((const uint8_t*)str + offset, len - offset);
        if ( written == 0 )
            return -1;  // indicate an error (< 0)
        offset += written;
    }

    return (int)offset;
}

/*
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
*/
