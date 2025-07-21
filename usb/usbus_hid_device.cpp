#include "usbus_hid_device.hpp" // Patch usbus/hid.h which is included by control.h below.

#include "log.h"
#include "usb/usbus/control.h"  // for usbus_control_* definitions
#include "ztimer.h"             // for ztimer_set(), ztimer_now() and ztimer_remove()



static size_t _gen_hid_descriptor(usbus_t* usbus, void* arg)
{
    usbus_hid_device_t* hid = (usbus_hid_device_t*)arg;
    usb_desc_hid_t hid_desc;

    hid_desc.length = sizeof(usb_desc_hid_t);
    hid_desc.desc_type = USB_HID_DESCR_HID;
    hid_desc.bcd_hid = USB_HID_VERSION_BCD;  // 1.10 (not 1.11?)
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
    tx_ready.handler = _hdlr_tx_ready;
    mutex_init(&in_lock);

    LOG_DEBUG("USB_HID: pre-init [%d] %d\n", report_desc[0], report_desc_size);
    usbus_register_event_handler(usbus, &handler_ctrl);
}

void usbus_hid_device_ext_t::_hdlr_tx_ready(event_t* event)
{
    usbus_hid_device_ext_t* const hidx = static_cast<usbus_hid_device_ext_t*>(
        (usbus_hid_device_t*)container_of(event, usbus_hid_device_t, tx_ready));
    usbdev_ep_xmit(hidx->ep_in->ep, hidx->in_buf, hidx->occupied);

    // The host typically polls for IN packets at intervals of at least
    // hidx->ep_in->interval ms. After sending a packet, we start tx_timer to monitor
    // for host responsiveness—for example, to detect a cable break or stalled
    // transmission.
    // An extra 1 ms is added as a timing margin to account for jitter.
    ztimer_set(ZTIMER_MSEC, &hidx->tx_timer, hidx->ep_in->interval + 1);
}

void usbus_hid_device_ext_t::_tmo_transfer_complete(void* arg)
{
    usbus_hid_device_ext_t* const hidx = static_cast<usbus_hid_device_ext_t*>(arg);
    LOG_WARNING("USB_HID: tx_timer expired!\n");
    usbus_event_post(hidx->usbus, &hidx->m_event_transfer_complete);
}

void usbus_hid_device_ext_t::_init(usbus_t* usbus, usbus_handler_t* handler)
{
    usbus_hid_device_ext_t* const hidx =
        static_cast<usbus_hid_device_ext_t*>((usbus_hid_device_t*)handler);
    LOG_DEBUG("USB_HID: initialization [%d]\n", hidx->report_desc[0]);

    hidx->hid_descr.next = nullptr;
    hidx->hid_descr.funcs = &_hid_descriptor;
    hidx->hid_descr.arg = dynamic_cast<usbus_hid_device_t*>(hidx);
    hidx->usb_init(usbus);
}

// Called from _usbdev_esr() in usbdev.c, within the usb_thread context.
void usbus_hid_device_ext_t::_event_handler(
    usbus_t*, usbus_handler_t* handler, usbus_event_usb_t event)
{
    usbus_hid_device_ext_t* const hidx =
        static_cast<usbus_hid_device_ext_t*>((usbus_hid_device_t*)handler);

    uint32_t now = ztimer_now(ZTIMER_MSEC);
    switch (event) {
        case USBUS_EVENT_USB_RESET:
            LOG_DEBUG("USB_HID: reset event @%lu\n", now);
            hidx->on_reset();
            break;

        case USBUS_EVENT_USB_SUSPEND:
            LOG_DEBUG("USB_HID: suspend event @%lu\n", now);
            hidx->on_suspend();
            break;

        case USBUS_EVENT_USB_RESUME:
            LOG_DEBUG("USB_HID: resume event @%lu\n", now);
            hidx->on_resume();
            break;

        default:
            break;
    }
}

void usbus_hid_device_ext_t::_transfer_handler(
    usbus_t*, usbus_handler_t* handler, usbdev_ep_t* ep, usbus_event_transfer_t)
{
    // LOG_DEBUG("USB_HID: transfer_handler\n");

    usbus_hid_device_ext_t* const hidx =
        static_cast<usbus_hid_device_ext_t*>((usbus_hid_device_t*)handler);

    if ( ep->type == USB_EP_TYPE_INTERRUPT ) {
        if ( ep->dir == USB_EP_DIR_IN ) {
            // "After the data packet and the CRC is sent to the host, the USB module
            // waits for an ACK handshake from the host. If an ACK handshake is not
            // received within 16 bit times, the USB module returns to idle and waits
            // for the next token packet. If an ACK handshake is successfully received
            // EPSTATUS.BK1RDY is cleared and EPINTFLAG.TRCPT1 is set."

            // This code executes when EPINTFLAG.TRCPT1 is set after the host responds
            // with ACK. (No need to call _ep_unready(hidx->ep_in->ep) in this case; the
            // usbus driver handles it automatically.)
            ztimer_remove(ZTIMER_MSEC, &hidx->tx_timer);
            // LOG_DEBUG("USB_HID: --------\n");
            hidx->on_transfer_complete(true);
        }
        else {
            size_t len;
            usbdev_ep_get(ep, USBOPT_EP_AVAILABLE, &len, sizeof(size_t));
            if ( likely(len > 0 && hidx->cb) )
                hidx->cb(dynamic_cast<usbus_hid_device_t*>(hidx), hidx->out_buf, len);
            usbdev_ep_xmit(ep, hidx->out_buf, 0);
        }
    }
}

int usbus_hid_device_ext_t::_control_handler(
    usbus_t* usbus, usbus_handler_t* handler,
    usbus_control_request_state_t state, usb_setup_t* setup)
{
    usbus_hid_device_ext_t* const hidx =
        static_cast<usbus_hid_device_ext_t*>((usbus_hid_device_t*)handler);

    LOG_DEBUG("USB_HID: request: 0x%x type: 0x%x value: %d length: %d state: %d\n",
          setup->request, setup->type, setup->value >> 8, setup->length, state);

    // Requests defined in USB HID 1.11 spec section 7
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

        case USB_HID_REQUEST_SET_REPORT:
            if ( state == USBUS_CONTROL_REQUEST_STATE_OUTDATA ) {
                size_t size = 0;
                uint8_t* data = usbus_control_get_out_data(usbus, &size);
                if ( size > 0 && hidx->cb )
                    hidx->cb(dynamic_cast<usbus_hid_device_t*>(hidx), data, size);
            }
            break;

        case USB_HID_REQUEST_GET_IDLE:
            if ( setup->length == 1 )
                usbus_control_slicer_put_char(usbus, 0);  // Send idle rate of 0.
            break;

        case USB_HID_REQUEST_SET_IDLE:
            // Handle HID SET_IDLE request:
            // - If the upper byte of wValue is 0, the idle duration is indefinite. In
            //   this case, the endpoint suppresses periodic reporting and sends data
            //   only when there's a change in the report content.
            // - We currently ignore the lower byte (Report ID), since this
            //   implementation supports only indefinite idle rates.
            // Todo: To support non-zero idle rates, start/restart a one-shot timer each
            // time a report is sent or when a SET_IDLE request is received. When the
            // timer expires, re-send the last report periodically, according to
            // hidx->ep_in->interval.
            if ( (setup->value >> 8) != 0 )  // MSB(wValue)
                return -1;  // signal stall to indicate unsupported (See _recv_setup())
            break;

        case USB_HID_REQUEST_GET_PROTOCOL:
            if ( setup->length == 1 ) {
                const uint8_t protocol = hidx->get_protocol();
                usbus_control_slicer_put_char(usbus, protocol);
                LOG_DEBUG("USB_HID: report protocol=%d\n", protocol);
            }
            break;

        case USB_HID_REQUEST_SET_PROTOCOL: {
            const uint8_t protocol = (uint8_t)setup->value;  // LSB(wValue)
            hidx->set_protocol(protocol);
            LOG_DEBUG("USB_HID: set protocol=%d\n", protocol);
            break;
        }

        default:
            LOG_WARNING("USB_HID: unknown request %d\n", setup->request);
            return -1;
    }

    return 1;
}
