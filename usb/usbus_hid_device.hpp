#pragma once

#include "sam_usb.h"            // for sam0_common_usb_t
#define class _class
// Riot's sys/include/usb/descriptor.h uses "class" as a struct member name.
#include "usb/usbus.h"          // Patch hid.h below.
#undef class
#include "usb/usbus/hid.h"      // for usbus_hid_device_t, usbus_hid_cb_t
#include "xtimer.h"             // for xtimer_t



class usbus_hid_device_ext_t: public usbus_hid_device_t {
public:
    // Set up usbus interrupt endpoint(s) for the device.
    virtual void init(usbus_t* usbus) =0;

    // Note that these will be called on those devices that register the corresponding
    // events in their init() using usbus_handler_set_flag().
    virtual void on_reset() {}
    virtual void on_suspend() {}
    virtual void on_resume() {}

    // When an (interrupt) transfer is made to the host the host may respond with ACK
    // (on_transfer_complete() is called), or may not respond (isr_on_transfer_timeout()
    // is called).
    virtual void on_transfer_complete() =0;
    virtual void isr_on_transfer_timeout() =0;

    // Only Boot protocol is reported unless overridden.
    virtual uint8_t get_protocol() const { return 0; };
    virtual void set_protocol(uint8_t) {}

    // Check if usbus is active, not suspending nor resuming.
    // It seems that usbus_is_active() and usbhub_is_active() behave the same, only
    // differ in who detects it.
    bool usbus_is_active() const {
        return usbus->state == USBUS_STATE_CONFIGURED;
        // Or, we could read directly from FSMSTATUS:
        // return ((sam0_common_usb_t*)usbus->dev)->config->device->FSMSTATUS.reg
        //     == USB_FSMSTATUS_FSMSTATE_ON;
    }

    usbus_hid_device_ext_t(const usbus_hid_device_ext_t&) =delete;
    usbus_hid_device_ext_t& operator=(const usbus_hid_device_ext_t&) =delete;

protected:
    usbus_hid_device_ext_t(usbus_t* usbus,
        const uint8_t* report_desc, size_t report_desc_size, usbus_hid_cb_t cb_receive_data);

    // Will be never used.
    virtual ~usbus_hid_device_ext_t() { xtimer_remove(&tx_timer); }

private:
    xtimer_t tx_timer;

    static void _hdlr_tx_ready(event_t* event);
    static void _tmo_transfer_timeout(void* arg);

    static void _init(usbus_t* usbus, usbus_handler_t* handler);
    static void _event_handler(usbus_t* usbus, usbus_handler_t* handler,
                    usbus_event_usb_t event);
    static void _transfer_handler(usbus_t* usbus, usbus_handler_t* handler,
                    usbdev_ep_t* ep, usbus_event_transfer_t event);
    static int _control_handler(usbus_t* usbus, usbus_handler_t* handler,
                    usbus_control_request_state_t state, usb_setup_t* setup);

    static inline const usbus_handler_driver_t _hid_driver = {
        .init = _init,
        .event_handler = _event_handler,
        .transfer_handler = _transfer_handler,
        .control_handler = _control_handler,
    };
};
