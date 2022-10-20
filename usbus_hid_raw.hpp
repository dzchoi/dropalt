#pragma once

#include "usb_descriptor.hpp"
#include "usbus_hid_device.hpp"



template <bool>
// the default bogus driver
class usbus_hid_raw_tl {
public:
    usbus_hid_raw_tl(usbus_t* usbus) { (void)usbus; }

    // As a bogus driver it is not registered as hid driver, no need to handle init().

    int puts(const char* str) { (void)str; return -1; }
};

template<>
class usbus_hid_raw_tl<true>: public usbus_hid_device_tl<MANUAL_REPORTING> {
public:
    static constexpr auto REPORT_DESC = array_of(RawReportDescriptor);

    static constexpr size_t RAW_EPSIZE = RAW_REPORT_SIZE;

    // Todo: For now it uses only "RAW_USAGE_PAGE = 0xFF31", which seems not supporting
    //   gets(), i.e. receiving data from host. We need to connect cb_receive_data to
    //   gets() to support the functionality.
    usbus_hid_raw_tl(usbus_t* usbus)
    : usbus_hid_device_tl(usbus, REPORT_DESC.data(), REPORT_DESC.size(), nullptr) {}

    void init(usbus_t* usbus);

    int puts(const char* str);
};
