#include "usbus_hid_device.hpp" // Patch hid.h that are included by control.h below.

#include "usb/usbus/control.h"  // for usbus_control_* definitions
#define ENABLE_DEBUG    (1)
#include "debug.h"



static void _init(usbus_t* usbus, usbus_handler_t* handler);
static void _event_handler(usbus_t* usbus, usbus_handler_t* handler,
                           usbus_event_usb_t event);
static void _transfer_handler(usbus_t* usbus, usbus_handler_t* handler,
                              usbdev_ep_t* ep, usbus_event_transfer_t event);
static int _control_handler(usbus_t* usbus, usbus_handler_t* handler,
                            usbus_control_request_state_t state,
                            usb_setup_t* setup);



static const usbus_handler_driver_t _hid_driver = {
    .init = _init,
    .event_handler = _event_handler,
    .transfer_handler = _transfer_handler,
    .control_handler = _control_handler,
};

static size_t _gen_hid_descriptor(usbus_t* usbus, void* arg)
{
    usbus_hid_device_t* hid = (usbus_hid_device_t*)arg;
    usb_desc_hid_t hid_desc;

    hid_desc.length = sizeof(usb_desc_hid_t);
    hid_desc.desc_type = USB_HID_DESCR_HID;
    hid_desc.bcd_hid = USB_HID_VERSION_BCD;  // 1.10 (not 1.11???)
    hid_desc.country_code = USB_HID_COUNTRY_CODE_NOTSUPPORTED;
    hid_desc.num_descrs = 0x01;
    hid_desc.report_type = USB_HID_DESCR_REPORT;
    hid_desc.report_length = hid->report_desc_size;

    usbus_control_slicer_put_bytes(usbus, (uint8_t*)&hid_desc, sizeof(hid_desc));
    return sizeof(usb_desc_hid_t);
}

static const usbus_descr_gen_funcs_t _hid_descriptor = {
    .fmt_pre_descriptor = nullptr,
    .fmt_post_descriptor = _gen_hid_descriptor,
    .len = { .fixed_len = sizeof(usb_desc_hid_t) },
    .len_type = USBUS_DESCR_LEN_FIXED
};

static void _hdlr_tx_ready(event_t* ev)
{
    usbus_hid_device_ext_t* const hidx = static_cast<usbus_hid_device_ext_t*>(
        (usbus_hid_device_t*)container_of(ev, usbus_hid_device_t, tx_ready));
    usbdev_ep_xmit(hidx->ep_in->ep, hidx->in_buf, hidx->occupied);

    if ( !hidx->is_automatic_reporting() )
        // The host is supposed to read IN packets once in every hid->ep_in->interval ms
        // at least. When we send packets we also start tx_timer so that we can check if
        // the host is responsive or not (e.g. cable break while sending).
        xtimer_set(
            &hidx->tx_timer,
            (hidx->ep_in->interval + 1) *US_PER_MS);  // +1 ms for margin
}

// Send the (automatic) report in usb_thread context.
static void _tmo_automatic_report(void* arg)
{
    usbus_hid_device_tl<AUTOMATIC_REPORTING>* const hidx =
        static_cast<usbus_hid_device_tl<AUTOMATIC_REPORTING>*>(arg);
    xtimer_set(&hidx->tx_timer, hidx->ep_in->interval *US_PER_MS);

    // Note that hidx->changed is used like a mutex. As we are now in ISR it cannot be
    // changed until we are done.
    if ( !hidx->changed )
        return;

    // If USB is not active we drop the report. Otherwise, the usbus driver would wait
    // forever or freeze.
    if ( hidx->usbus_is_active() && mutex_trylock(&hidx->in_lock) != 0 ) {
        hidx->_isr_fill_in_buf();  // Fill in hidx->in_buf.
        hidx->occupied = hidx->ep_in->maxpacketsize;
        usbus_event_post(hidx->usbus, &hidx->tx_ready);
        hidx->changed = false;
    } else {
        // If hidx->in_lock is already locked (for hidx->ep_in->interval ms) we drop the
        // report instead of waiting for it to be unlocked indefinitely.
        hidx->changed = false;
        hidx->occupied = 0;
        mutex_unlock(&hidx->in_lock);
        DEBUG("USB_HID:\e[0;31m data packet not sent!\e[0m\n");
    }

    // Notify of report being done when hidx->changed becomes false.
    hidx->_isr_report_done();
}

static void _tmo_tx_timer_expired(void* arg)
{
    // It's not a good situation when hidx->tx_timer expires, which means we might lose
    // an IN packet that is to send to the host.
    usbus_hid_device_tl<MANUAL_REPORTING>* const hidx =
        static_cast<usbus_hid_device_tl<MANUAL_REPORTING>*>(arg);
    hidx->occupied = 0;
    mutex_unlock(&hidx->in_lock);
    DEBUG("USB_HID: tx_timer expired!\n");
}

static void _init(usbus_t* usbus, usbus_handler_t* handler)
{
    usbus_hid_device_ext_t* const hidx =
        static_cast<usbus_hid_device_ext_t*>((usbus_hid_device_t*)handler);
    DEBUG("USB_HID: initialization (%d)\n", hidx->report_desc[0]);

    hidx->hid_descr.next = nullptr;
    hidx->hid_descr.funcs = &_hid_descriptor;
    hidx->hid_descr.arg = dynamic_cast<usbus_hid_device_t*>(hidx);
    hidx->init(usbus);

    hidx->tx_ready.handler = _hdlr_tx_ready;
    hidx->tx_timer.arg = hidx;
    if ( hidx->is_automatic_reporting() ) {
        hidx->tx_timer.callback = _tmo_automatic_report;
        // Start automatic reporting.
        xtimer_set(&hidx->tx_timer, hidx->ep_in->interval *US_PER_MS);
    } else
        hidx->tx_timer.callback = _tmo_tx_timer_expired;
}

static void _event_handler(usbus_t* usbus, usbus_handler_t* handler,
                           usbus_event_usb_t event)
{
    (void)usbus;
    usbus_hid_device_ext_t* const hidx =
        static_cast<usbus_hid_device_ext_t*>((usbus_hid_device_t*)handler);

    switch (event) {
        // "A line reset is a host initiated USB reset to the peripheral"
        case USBUS_EVENT_USB_RESET:     // USB reset event
            // When the host is booting, it first sends SET_PROTOCOL on BIOS screen to set
            // boot protocol, then sends USB_RESET on entering OS to set it back to report
            // protocol.
            hidx->on_reset();
            DEBUG("USB_HID: reset event\n");
            break;

        // "The Suspend Interrupt bit in the Device Interrupt Flag register (INTFLAG
        // SUSPEND) is set when a USB Suspend state has been detected on the USB bus.
        // The USB pad is then automatically put in the Idle state. The detection of a
        // non-idle state sets the Wake Up Interrupt bit (INTFLAG.WAKEUP) and wakes
        // the USB pad.
        // The pad goes to the Idle state if the USB module is disabled or if CTRLB.
        // DETACH is written to one. It returns to the Active state when CTRLA.ENABLE is
        // written to one and CTRLB.DETACH is written to zero."

        case USBUS_EVENT_USB_SUSPEND:   // USB suspend condition detected
            hidx->on_suspend();
            DEBUG("USB_HID: suspend event\n");
            break;

        case USBUS_EVENT_USB_RESUME:    // USB resume condition detected
            hidx->on_resume();
            DEBUG("USB_HID: resume event\n");
            break;

        default:
            break;
    }
}

static void _transfer_handler(usbus_t* usbus, usbus_handler_t* handler,
                              usbdev_ep_t* ep, usbus_event_transfer_t event)
{
    (void)usbus;
    (void)event;
    // DEBUG("USB_HID: transfer_handler\n");  // Would be emitted every printf().

    usbus_hid_device_ext_t* const hidx =
        static_cast<usbus_hid_device_ext_t*>((usbus_hid_device_t*)handler);

    if ( ep->type == USB_EP_TYPE_INTERRUPT ) {
        if ( ep->dir == USB_EP_DIR_IN ) {
            // "After the data packet and the CRC is sent to the host, the USB module
            // waits for an ACK handshake from the host. If an ACK handshake is not
            // received within 16 bit times, the USB module returns to idle and waits
            // for the next token packet. If an ACK handshake is successfully received
            // EPSTATUS.BK1RDY is cleared and EPINTFLAG.TRCPT1 is set."

            // This code is called when EPINTFLAG.TRCPT1 is set after an ACK handshake
            // is received from the host. Note that we don't need to execute
            // _ep_unready(hid->ep_in->ep) here, since it is already done automatically
            // by the usbus driver.

            if ( !hidx->is_automatic_reporting() )
                xtimer_remove(&hidx->tx_timer);
            hidx->occupied = 0;
            mutex_unlock(&hidx->in_lock);
        }
        else {
            size_t len;
            usbdev_ep_get(ep, USBOPT_EP_AVAILABLE, &len, sizeof(size_t));
            if ( len > 0 && hidx->cb )
                hidx->cb(dynamic_cast<usbus_hid_device_t*>(hidx), hidx->out_buf, len);
            usbdev_ep_xmit(ep, hidx->out_buf, 0);
        }
    }
}

static int _control_handler(usbus_t* usbus, usbus_handler_t* handler,
                            usbus_control_request_state_t state,
                            usb_setup_t* setup)
{
    usbus_hid_device_ext_t* const hidx =
        static_cast<usbus_hid_device_ext_t*>((usbus_hid_device_t*)handler);

    DEBUG("USB_HID: request: %d type: %d value: %d length: %d state: %d\n",
          setup->request, setup->type, setup->value >> 8, setup->length, state);

    /* Requests defined in USB HID 1.11 spec section 7 */
    switch ( setup->request ) {
        case USB_SETUP_REQ_GET_DESCRIPTOR: {
            const uint8_t desc_type = setup->value >> 8;
            if ( desc_type == USB_HID_DESCR_REPORT ) {
                usbus_control_slicer_put_bytes(usbus, hidx->report_desc,
                                               hidx->report_desc_size);
            }
            else if ( desc_type == USB_HID_DESCR_HID ) {
                _gen_hid_descriptor(usbus, nullptr);
            }
            break;
        }

        case USB_HID_REQUEST_GET_REPORT:
            // Todo: Resend the last keyboard report, or a blank mouse report.
            break;

        case USB_HID_REQUEST_GET_IDLE:
            if ( setup->length == 1 )
                usbus_control_slicer_put_char(usbus, 0);  // send idle rate of 0.
            break;

        case USB_HID_REQUEST_GET_PROTOCOL:
            if ( setup->length == 1 ) {
                const uint8_t protocol = hidx->get_protocol();
                usbus_control_slicer_put_char(usbus, protocol);
                DEBUG("USB_HID: report protocol=%d\n", protocol);
            }
            break;

        case USB_HID_REQUEST_SET_REPORT:
            if ( state == USBUS_CONTROL_REQUEST_STATE_OUTDATA ) {
                size_t size = 0;
                uint8_t* data = usbus_control_get_out_data(usbus, &size);
                if ( size > 0 && hidx->cb )
                    hidx->cb(dynamic_cast<usbus_hid_device_t*>(hidx), data, size);
            }
            break;

        case USB_HID_REQUEST_SET_IDLE:
            // "When the upper byte of wValue is 0 (zero), the duration is indefinite.
            // The endpoint will inhibit reporting forever, only reporting when a
            // change is detected in the report data."

            // We do not check the report id in the lower byte of setup->value, as we
            // only support an indefinite duration for all hid interfaces.
            // Todo: To support idle rate, set/reset a onetime timer with the timeout of
            //   idle rate (ms) everytime send_keyboard() is invoked or when receiving
            //   SET_IDLE. Then when the timer is expired send the last report afterwards
            //   at every hid->ep_in->interval ms.
            if ( (setup->value >> 8) != 0 )  // MSB(wValue)
                return -1;  // signal stall to indicate unsupported (See _recv_setup())
            break;

        case USB_HID_REQUEST_SET_PROTOCOL: {
            // "The SETUP packet's request type would contain 0x21, the request code for
            // SetProtocol is 0x0B, and the value field of the SETUP packet should
            // contain 0 to indicate boot protocol, or 1 to indicate report protocol."
            const uint8_t protocol = (uint8_t)setup->value;  // LSB(wValue)
            hidx->set_protocol(protocol);
            DEBUG("USB_HID: set protocol=%d\n", protocol);
            break;
        }

        default:
            DEBUG("USB_HID: unknown request %d \n", setup->request);
            return -1;
    }

    return 1;
}



usbus_hid_device_ext_t::usbus_hid_device_ext_t(usbus_t* _usbus,
    const uint8_t* _report_desc, size_t _report_desc_size, usbus_hid_cb_t cb_receive_data)
{
    __builtin_memset(
        dynamic_cast<usbus_hid_device_t*>(this), 0, sizeof(usbus_hid_device_t));

    handler_ctrl.driver = &_hid_driver;
    report_desc = _report_desc;
    report_desc_size = _report_desc_size;
    usbus = _usbus;
    cb = cb_receive_data;
    mutex_init(&in_lock);

    DEBUG("hid pre_init: %d %d\n", report_desc_size, report_desc[0]);
    usbus_register_event_handler(usbus, &handler_ctrl);
}

void usbus_hid_device_tl<MANUAL_REPORTING>::flush()
{
    mutex_lock(&in_lock);  // waits for mutex unlocked.
    mutex_unlock(&in_lock);
}

size_t usbus_hid_device_tl<MANUAL_REPORTING>::submit(const uint8_t* buf, size_t len)
{
    // If USB is not active we drop the given submit. Otherwise, the usbus driver would
    // wait forever or freeze.
    if ( !usbus_is_active() )
        return 0;

    const size_t maxpacketsize = ep_in->maxpacketsize;
    assert( maxpacketsize == ep_in->ep->len );

    if ( len > maxpacketsize )
        len = maxpacketsize;

    // Even if USB is active and our tx is ready, the host may not be able to consume the
    // data packet (e.g. executing console_printf() without hid_listen running on the
    // host). So instead of waiting indefinitely for a previous packet to be delivered,
    // we wait here no longer than this->ep_in->interval ms. See _tmo_tx_timer_expired()
    // and _transfer_handler().
    mutex_lock(&in_lock);
    __builtin_memcpy(in_buf, buf, len);
    occupied = len;
    usbus_event_post(usbus, &tx_ready);
    return len;
}



/*
#if defined(MOUSE_ENABLE) && !defined(MOUSE_SHARED_EP)
void usbus_hid_mouse_t::init(usbus_t* usbus)
{
    iface._class = USB_CLASS_HID;
    iface.subclass = USB_HID_SUBCLASS_BOOT;
    iface.protocol = USB_HID_PROTOCOL_MOUSE;
    iface.descr_gen = &hid_descr;
    iface.handler = &handler_ctrl;

    // Register the events that the hid will listen to.
    usbus_handler_set_flag(&handler_ctrl,
        USBUS_HANDLER_FLAG_RESET
        | USBUS_HANDLER_FLAG_SUSPEND
        | USBUS_HANDLER_FLAG_RESUME);

    // IN endpoint to send data to host
    ep_in = usbus_add_endpoint(usbus, &iface,
                               USB_EP_TYPE_INTERRUPT,
                               USB_EP_DIR_IN,
                               MOUSE_EPSIZE);
    // interrupt endpoint polling rate in ms
    ep_in->interval = USB_POLLING_INTERVAL_MS;
    usbus_enable_endpoint(ep_in);

    usbus_add_interface(usbus, &iface);
}
#endif

#ifdef SHARED_EP_ENABLE
void usbus_hid_shared_t::init(usbus_t* usbus)
{
    iface._class = USB_CLASS_HID;
#ifdef KEYBOARD_SHARED_EP
    iface.subclass = USB_HID_SUBCLASS_BOOT;
    iface.protocol = USB_HID_PROTOCOL_KEYBOARD;
#else
    // Configure generic HID device interface, choosing NONE for subclass and protocol
    // in order to represent a generic I/O device
    iface.subclass = USB_HID_SUBCLASS_NONE;
    iface.protocol = USB_HID_PROTOCOL_NONE;
#endif
    iface.descr_gen = &hid_descr;
    iface.handler = &handler_ctrl;

#ifdef KEYBOARD_SHARED_EP
    // Register the events that the hid will listen to.
    usbus_handler_set_flag(&handler_ctrl,
        USBUS_HANDLER_FLAG_RESET
        | USBUS_HANDLER_FLAG_SUSPEND
        | USBUS_HANDLER_FLAG_RESUME);
#endif

    // IN endpoint to send data to host
    ep_in = usbus_add_endpoint(usbus, &iface,
                               USB_EP_TYPE_INTERRUPT,
                               USB_EP_DIR_IN,
                               SHARED_EPSIZE);
    // interrupt endpoint polling rate in ms
    ep_in->interval = USB_POLLING_INTERVAL_MS;
    usbus_enable_endpoint(ep_in);

    usbus_add_interface(usbus, &iface);
}
#endif
*/
