#include "usbus_hid_ext.hpp"    // Patch hid.h that are included by control.h below.

#include "usb/usbus/control.h"  // for usbus_control_* definitions
#define ENABLE_DEBUG    (1)
#include "debug.h"

// #include "_keymap_config.h"     // Todo: NKRO_ENABLE
#include "main_thread.hpp"
#include "usb_thread.hpp"



uint8_t keyboard_protocol = 1;

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
    .len = {
        .fixed_len = sizeof(usb_desc_hid_t)
    },
    .len_type = USBUS_DESCR_LEN_FIXED
};

static void _handle_tx_ready(event_t* ev)
{
    usbus_hid_device_ext_t* const hidx = static_cast<usbus_hid_device_ext_t*>(
        (usbus_hid_device_t*)container_of(ev, usbus_hid_device_t, tx_ready));
    usbdev_ep_xmit(hidx->ep_in->ep, hidx->in_buf, hidx->occupied);
    xtimer_set(
        &hidx->tx_timer,
        // The host is supposed to read IN packets once in every hid->ep_in->interval
        // ms at least. We give it one more ms for a safety margin (our xtimer itself
        // seems to have an in-accuracy of +/- 100 us.)
        (hidx->ep_in->interval + 1) *MS_PER_SEC);
}

static void _tx_timer_expired(void* arg)
{
    // It's not a good situation when hidx->tx_timer expires. It means we can possibly
    // miss an IN packet that is to send to the host.
    DEBUG("USB_HID: tx_timer expired!\n");
    usbus_hid_device_ext_t* const hidx = static_cast<usbus_hid_device_ext_t*>(arg);
    hidx->occupied = 0;
    mutex_unlock(&hidx->in_lock);
}

static void _init(usbus_t* usbus, usbus_handler_t* handler)
{
    usbus_hid_device_ext_t* const hidx =
        static_cast<usbus_hid_device_ext_t*>((usbus_hid_device_t*)handler);
    DEBUG("USB_HID: initialization (%d)\n", hidx->report_desc[0]);

    hidx->tx_ready.handler = _handle_tx_ready;
    hidx->hid_descr.next = nullptr;
    hidx->hid_descr.funcs = &_hid_descriptor;
    hidx->hid_descr.arg = dynamic_cast<usbus_hid_device_t*>(hidx);

    hidx->init(usbus);
}

static void _event_handler(usbus_t* usbus, usbus_handler_t* handler,
                           usbus_event_usb_t event)
{
    (void)usbus;
    (void)handler;

    switch (event) {
        case USBUS_EVENT_USB_RESET:     // USB reset event
            // When PC is booting, PC first does SET_PROTOCOL on BIOS screen to set boot
            // protocol, then sends USB_RESET on entering OS to set it back to report
            // protocol.
            keyboard_protocol = 1;
#if defined(NKRO_ENABLE) && defined(FORCE_NKRO)
            keymap_config.nkro = 1;
            // eeconfig_update_keymap(keymap_config.raw);  // Todo: ???
#endif
            // The "main" thread does not monitor the flag yet.
            // main_thread::obj().signal_usb_reset();
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
            main_thread::obj().signal_usb_suspend();
            break;

        case USBUS_EVENT_USB_RESUME:    // USB resume condition detected
            main_thread::obj().signal_usb_resume();
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

            // This code block is invoked when EPINTFLAG.TRCPT1 is set after the ACK
            // handshake with the host, that is, after the host acknowledges the packet
            // is received by the host. Note that we don't need to
            // execute _ep_unready(hid->ep_in->ep) here, since it is already done
            // automatically by the USB controller.
            xtimer_remove(&hidx->tx_timer);
            hidx->occupied = 0;  // Or, we could call _tx_timer_expired(hidx) instead.
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
            uint8_t desc_type = setup->value >> 8;
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
            if ( hidx->is_keyboard() )
                if ( setup->length == 1 ) {
                    usbus_control_slicer_put_char(usbus, keyboard_protocol);
                    DEBUG("USB_HID: report keyboard_protocol=%d\n", keyboard_protocol);
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
            // Todo: Supporting idle rate is not so difficult though. (Re-)set a timer
            //   with the timeout of idle rate (ms) everytime send_keyboard() is invoked
            //   or when receiving SET_IDLE, and then after the timer is expired send
            //   the last report at every USB_POLLING_INTERVAL_MS (ms).
            if ( (setup->value >> 8) != 0 )  // MSB(wValue)
                return -1;  // signal stall to indicate unsupported (See _recv_setup())
            break;

        case USB_HID_REQUEST_SET_PROTOCOL:
            // "The SETUP packet's request type would contain 0x21, the request code for
            // SetProtocol is 0x0B, and the value field of the SETUP packet should
            // contain 0 to indicate boot protocol, or 1 to indicate report protocol."
            if ( hidx->is_keyboard() ) {
                keyboard_protocol = (uint8_t)setup->value;  // LSB(wValue)
                DEBUG("USB_HID: set keyboard_protocol=%d\n", keyboard_protocol);
#ifdef NKRO_ENABLE
                keymap_config.nkro = (keyboard_protocol != 0);
#endif
            }
            break;

        default:
            DEBUG("USB_HID: unknown request %d \n", setup->request);
            return -1;
    }

    return 1;
}



void usbus_hid_device_ext_t::pre_init(usbus_t* usbus,
    const uint8_t* report_desc, size_t report_desc_size, usbus_hid_cb_t cb)
{
    usbus_hid_device_ext_t::usbus = usbus;
    mutex_init(&in_lock);
    handler_ctrl.driver = &_hid_driver;
    usbus_hid_device_ext_t::report_desc = report_desc;
    usbus_hid_device_ext_t::report_desc_size = report_desc_size;
    usbus_hid_device_ext_t::cb = cb;
    tx_timer.callback = _tx_timer_expired;
    tx_timer.arg = this;

    DEBUG("hid pre_init: %d %d\n", report_desc_size, report_desc[0]);
    usbus_register_event_handler(usbus, &handler_ctrl);
}

void usbus_hid_device_ext_t::flush()
{
    mutex_lock(&in_lock);  // waits for mutex unlocked.
    mutex_unlock(&in_lock);
}

// Note: It can block for a this->ep_in->interval ms at maximum (on the mutex_lock()
//   below) if there is on-going or pending transmit already. (Trying to avoid this
//   blocking by appending the incoming packet data to the transmit buffer which is yet
//   to send didn't work, and resulted in missing packets sometimes.)
size_t usbus_hid_device_ext_t::submit(const uint8_t* buf, size_t len)
{
    // If USB is not active (i.e. not USBUS_STATE_CONFIGURED) we drop the given submit.
    // Otherwise, the USB controller would wait forever or freeze.
    // We could use a stronger condition to check if the USB controller's finite state
    // machine is in ON (=0x02) state; not SUSPEND (=0x04), UPRESUME (0x20) or
    // RESET (0x40) states:
    //   `((sam0_common_usb_t*)this->usbus->dev)->config->device->FSMSTATUS.reg != 0x02`
    // But we can still catch the possibly slipping out cases with the mutex_lock() below.
    if ( usbus->state != USBUS_STATE_CONFIGURED )
        return 0;

    const size_t maxpacketsize = ep_in->maxpacketsize;
    assert(maxpacketsize == ep_in->ep->len);

    if ( len > maxpacketsize )
        len = maxpacketsize;

    // Even if USB is active and our tx is ready, the host may not be able to read the
    // data packet (e.g. executing console_printf() without running hid_listen on the
    // host). For this case, instead of waiting indefinitely for the packet to be
    // delivered, we wait for only this->ep_in->interval ms after readying tx. See
    // _tx_timer_expired() and _transfer_handler() for how we wait in this manner and
    // how the mutex is unlocked eventually in this->ep_in->interval ms.
    mutex_lock(&in_lock);
    memcpy(in_buf, buf, len);
    occupied = len;
    usbus_event_post(usbus, &tx_ready);
    return len;
}



#ifndef KEYBOARD_SHARED_EP
void usbus_hid_keyboard_t::init(usbus_t* usbus)
{
    iface._class = USB_CLASS_HID;
    iface.subclass = USB_HID_SUBCLASS_BOOT;
    iface.protocol = USB_HID_PROTOCOL_KEYBOARD;
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
                               KEYBOARD_EPSIZE);
    // interrupt endpoint polling rate in ms
    ep_in->interval = USB_POLLING_INTERVAL_MS;
    usbus_enable_endpoint(ep_in);

    // It will return the bInterfaceNumber of the added interface, which can also
    // be read using hid->iface->idx, but we will unlikely need it since all operations
    // will provide the hid that it oprates on.
    usbus_add_interface(usbus, &iface);
}
#endif

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

#ifdef RAW_ENABLE
void usbus_hid_raw_t::init(usbus_t* usbus)
{
    // Todo: UsageRaw
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
    ep_in->interval = 1;  // 1 ms of interrupt endpoint polling rate
    usbus_enable_endpoint(ep_in);

    // OUT endpoint to receive data from host
    ep_out = usbus_add_endpoint(usbus, &iface,
                                USB_EP_TYPE_INTERRUPT,
                                USB_EP_DIR_OUT,
                                RAW_EPSIZE);
    ep_out->interval = 1;
    usbus_enable_endpoint(ep_out);

    // signal that INTERRUPT OUT is ready to receive data
    usbdev_ep_xmit(ep_out->ep, out_buf, 0);

    usbus_add_interface(usbus, &iface);
}
#endif

#ifdef CONSOLE_ENABLE
void usbus_hid_console_t::init(usbus_t* usbus)
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
                               CONSOLE_EPSIZE);
    ep_in->interval = 1;  // 1 ms of interrupt endpoint polling rate
    usbus_enable_endpoint(ep_in);

    usbus_add_interface(usbus, &iface);
}
#endif
