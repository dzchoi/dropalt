#pragma once

#include <atomic>

#include "sam_usb.h"            // for sam0_common_usb_t
#define class _class
// Riot's sys/include/usb/descriptor.h uses "class" as a struct member name.
#include "usb/usbus.h"          // Patch hid.h below.
#undef class
#include "usb/usbus/hid.h"      // for usbus_hid_device_t, usbus_hid_cb_t
#include "xtimer.h"             // for xtimer_t



struct usbus_hid_device_ext_t: usbus_hid_device_t {
    xtimer_t tx_timer;

    virtual bool is_automatic_reporting() =0;

    usbus_hid_device_ext_t(usbus_t* usbus,
        const uint8_t* report_desc, size_t report_desc_size, usbus_hid_cb_t cb_receive_data);

    // Will not be used.
    virtual ~usbus_hid_device_ext_t() { xtimer_remove(&tx_timer); }

    // Set up usbus interrupt endpoint(s) for the device.
    virtual void init(usbus_t* usbus) =0;

    // Note that these will be called on those devices that register the corresponding
    // events in init() using usbus_handler_set_flag().
    virtual void on_reset() {}
    virtual void on_suspend() {}
    virtual void on_resume() {}

    // Only Boot protocol is supported by default.
    virtual uint8_t get_protocol() const { return 0; };
    virtual void set_protocol(uint8_t protocol) { (void)protocol; }

    // Check if usbus is active, not suspending nor resuming.
    // It seems that usbus_is_active() and usbhub_is_active() are the same, only
    // differ in who detects it.
    bool usbus_is_active() const {
        // Instead of reading directly from FSMSTATUS, we could also use:
        // `return usbus->state == USBUS_STATE_CONFIGURED`.
        return ((sam0_common_usb_t*)usbus->dev)->config->device->FSMSTATUS.reg
            == USB_FSMSTATUS_FSMSTATE_ON;
    }

    usbus_hid_device_ext_t(const usbus_hid_device_ext_t&) =delete;
    usbus_hid_device_ext_t& operator=(const usbus_hid_device_ext_t&) =delete;
};



template <bool>
struct usbus_hid_device_tl;

constexpr bool MANUAL_REPORTING = false;
constexpr bool AUTOMATIC_REPORTING = true;

template <>
struct usbus_hid_device_tl<MANUAL_REPORTING>: usbus_hid_device_ext_t {
    bool is_automatic_reporting() { return false; }

    using usbus_hid_device_ext_t::usbus_hid_device_ext_t;

    void flush();

    // Send a data packet to the host manually. The max size of data that can be sent in
    // one transfer is limited by the IN endpoint size, returning the actual size that
    // has been sent. So, repeated calls in a loop until all data is sent can cause being
    // stuck when trying to send more data than the interrupt transfers can hold.
    // Note: Mostly this is non-blocking call, not waiting for the delivery to complete,
    //   unless the host is unresponsive, in which case it waits for
    //   this->ep_in->interval ms and drops the previous blocking packet and pushes the
    //   current packet.
    size_t submit(const uint8_t* buf, size_t len);
};

template <>
struct usbus_hid_device_tl<AUTOMATIC_REPORTING>: usbus_hid_device_ext_t {
    bool is_automatic_reporting() { return true; }

    using usbus_hid_device_ext_t::usbus_hid_device_ext_t;

    // Indicator if the report is changed since last report.
    std::atomic<bool> changed = false;

    // Fill in hidx->in_buf.
    // Note: After executing _isr_fill_in_buf() the hidx->occupied is always set to
    //   hidx->ep_in->maxpacketsize (i.e. *_REPORT_SIZE), making the entire report
    //   transferred to the host, whereas submit() transfers only up to the given `len`
    //   bytes of data.
    virtual void _isr_fill_in_buf() =0;

    // Called when report is sent to the host.
    virtual void _isr_report_done() {}
};
