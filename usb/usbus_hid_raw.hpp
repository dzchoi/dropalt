#pragma once

#include "usb_descriptor.hpp"
#include "usbus_hid_device.hpp"



template <bool>
// the default bogus driver
class usbus_hid_raw_tl {
public:
    usbus_hid_raw_tl(usbus_t*) {}

    // As a bogus driver it is not registered as hid driver, no need to provide init().

    int print(const char*) { return -1; }
};

template<>
class usbus_hid_raw_tl<true>: public usbus_hid_device_ext_t {
public:
    static constexpr size_t RAW_EPSIZE = RAW_REPORT_SIZE;
    static inline const auto report_desc = array_of(RawReportDescriptor);

    // Todo: For now it uses only "RAW_USAGE_PAGE = 0xFF31", which seems not supporting
    //   gets(), i.e. receiving data from host. We need to connect cb_receive_data to
    //   gets() to support the functionality.
    usbus_hid_raw_tl(usbus_t* usbus)
    : usbus_hid_device_ext_t(usbus, report_desc.data(), report_desc.size(), nullptr)
    {}

    void init(usbus_t* usbus);

    int print(const char* str);

private:
    // Send a data packet to the host through interrupt transfer. The max size of the
    // data payload that can be sent in one transfer is limited by the max packet size of
    // the IN endpoint, and it returns the actual size that has been sent.
    // So, repeated calls in a loop until all data is sent can result in being stuck when
    // trying to send more data than the interrupt transfers can handle.
    // Note: Mostly this is non-blocking call, not waiting for the delivery to the host
    //  to complete, unless the host is unresponsive, in which case it waits for
    //  this->ep_in->interval ms and drops the previous blocking packet and pushes the
    //  current packet.
    size_t submit(const uint8_t* buf, size_t len);

    void flush() {
        mutex_lock(&in_lock);  // waits for mutex unlocked.
        mutex_unlock(&in_lock);
    }

    void on_transfer_complete(bool was_successful);
};
