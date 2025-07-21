#pragma once

#include "usbus_ext.h"          // for usbus_t
#include "usb/usbus/hid.h"      // for usbus_hid_device_t, usbus_hid_cb_t
#include "ztimer.h"             // for ztimer_t and ztimer_remove()

#include "event_ext.hpp"        // for event_ext_t<>



class usbus_hid_device_ext_t: public usbus_hid_device_t {
public:
    // Set up usbus interrupt endpoints for the device.
    virtual void usb_init(usbus_t* usbus) =0;

    // These methods are triggered by usbus state changes initiated by the host:
    // - on_reset() is called when the USB controller detects SE0 (Single-Ended Zero,
    //   both D+ and D− low) for 10+ ms—typically during device enumeration or host
    //   reinitialization.
    // - on_suspend() is called when the controller detects a J-state (no bus activity,
    //   with D+ high) for 3+ ms—usually when the host enters sleep or shuts down.
    // - on_resume() is called when bus activity resumes, indicated by Start-of-Frame
    //   (SOF) packets transmitted by the host following a period of USB idle.
    virtual void on_reset() {}
    virtual void on_suspend() {}
    virtual void on_resume() {}

    // When an interrupt transfer is sent to the host, the host may respond with ACK
    // (on_transfer_complete(true) is called), or may fail to respond
    // (on_transfer_complete(false) is called).
    virtual void on_transfer_complete(bool was_successful) =0;

    // The host instructs the device which communication protocol to use:
    // - Boot Protocol: A simplified, fixed-format report used by BIOS or basic drivers,
    //   ensuring compatibility without parsing complex descriptors.
    // - Report Protocol: A flexible format defined by the device's HID report
    //   descriptor, allowing richer input features (e.g. media keys, NKRO).
    virtual uint8_t get_protocol() const { return 0; };  // Boot protocol by default
    virtual void set_protocol(uint8_t) {}

    // Check if usbus is active, not suspending nor resuming.
    // It seems that usbus_is_active() and usbhub_is_active() do the same thing, only
    // differs in which entity detects bus state.
    bool usbus_is_active() const {
        return usbus->state == USBUS_STATE_CONFIGURED;
        // Or we could read directly from FSMSTATUS:
        // return ((sam0_common_usb_t*)usbus->dev)->config->device->FSMSTATUS.reg
        //     == USB_FSMSTATUS_FSMSTATE_ON;
    }

    usbus_hid_device_ext_t(const usbus_hid_device_ext_t&) =delete;
    usbus_hid_device_ext_t& operator=(const usbus_hid_device_ext_t&) =delete;

protected:
    usbus_hid_device_ext_t(usbus_t* usbus,
        const uint8_t* report_desc, size_t report_desc_size, usbus_hid_cb_t cb_receive_data);

    event_ext_t<usbus_hid_device_ext_t*> m_event_transfer_complete = {
        nullptr,  // .list_node
        [](event_t* pevent) {  // .handler
            usbus_hid_device_ext_t* const hidx =
                static_cast<event_ext_t<usbus_hid_device_ext_t*>*>(pevent)->arg;
            hidx->on_transfer_complete(false);
        },
        this  // .arg
    };

    // Will be never used.
    // virtual ~usbus_hid_device_ext_t() { ztimer_remove(ZTIMER_MSEC, &tx_timer); }

private:
    ztimer_t tx_timer = { .callback = &_tmo_transfer_complete, .arg = this };

    static void _hdlr_tx_ready(event_t* event);
    static void _tmo_transfer_complete(void* arg);

    static void _init(usbus_t* usbus, usbus_handler_t* handler);
    static void _event_handler(usbus_t* usbus, usbus_handler_t* handler,
                    usbus_event_usb_t event);
    static void _transfer_handler(usbus_t* usbus, usbus_handler_t* handler,
                    usbdev_ep_t* ep, usbus_event_transfer_t event);
    static int _control_handler(usbus_t* usbus, usbus_handler_t* handler,
                    usbus_control_request_state_t state, usb_setup_t* setup);

    // All handlers are called from usb_thread context.
    static inline const usbus_handler_driver_t _hid_driver = {
        .init = _init,
        .event_handler = _event_handler,
        .transfer_handler = _transfer_handler,
        .control_handler = _control_handler,
    };
};
