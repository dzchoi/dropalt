#define ENABLE_DEBUG    (1)
#include "debug.h"

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

size_t usbus_hid_raw_tl<true>::submit(const uint8_t* buf, size_t len)
{
    // If USB is not active we drop the data packet, which will otherwise cause the
    // tx_timer expire eventually.
    if ( !usbus_is_active() )
        return 0;

    const size_t maxpacketsize = ep_in->maxpacketsize;
    assert( maxpacketsize == ep_in->ep->len );

    if ( len > maxpacketsize )
        len = maxpacketsize;

    // Even if USB is active and our tx is ready, the host may not be able to consume the
    // data packet (e.g. executing console print without hid_listen running on the host).
    // So instead of waiting indefinitely for a previous packet to be delivered, we wait
    // here no longer than this->ep_in->interval ms. See _tmo_transfer_timeout() and
    // _transfer_handler().
    mutex_lock(&in_lock);
    __builtin_memcpy(in_buf, buf, len);
    occupied = len;
    usbus_event_post(usbus, &tx_ready);
    return len;
}

void usbus_hid_raw_tl<true>::on_transfer_complete()
{
    occupied = 0;
    mutex_unlock(&in_lock);
}

void usbus_hid_raw_tl<true>::isr_on_transfer_timeout()
{
    on_transfer_complete();
    DEBUG("USB_HID:\e[0;31m tx_timer expired!\e[0m\n");
}

int usbus_hid_raw_tl<true>::print(const char* str)
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
