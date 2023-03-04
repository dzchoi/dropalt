#pragma once

#include <stdint.h>             // for uint*_t
#include "seeprom.h"
#include "usb2422.h"            // for USB_PORT_1
#include "ztimer.h"             // for ztimer_t

#include "color.hpp"            // for hsv_t
#include "features.hpp"         // for NVM_MAGIC_NUMBER, NVM_WRITE_DELAY_MS



class __attribute__((packed)) persistent {
public:
    static persistent& obj() {
        static persistent obj;
        return obj;
    }

    // Each data member is initialized with its default value first but is updated with
    // its persistent value in NVM as soon as obj() is called. To update the persistent
    // value execute write() after updating the member variable.

    uint32_t magic_number = NVM_MAGIC_NUMBER;
    uint16_t layout_version = 1u + (1u << 8);  // major version and minor version

    // Todo: Effect instead of a color?
    hsv_t led_color = hsv_t{ ORANGE, 255, 255 };

    uint8_t last_host_port = USB_PORT_1;

    // Write the data pointed by p into NVM (i.e. SmartEEPROM). To save the wearing out
    // only the changed parts are written, and written only after NVM_WRITE_DELAY_MS.
    // However, some writes can be immediate and not delayed (see the comment in
    // seeprom.h).
    template <typename T>
    void write(T persistent::* p);

    template <typename T>
    void write(T persistent::* p, const T& value) {
        this->*p = value;
        write(p);
    }

private:
    persistent();

    template <typename T>
    intptr_t offset(T persistent::* p) {
        return (uint8_t*)&(this->*p) - (uint8_t*)&this->magic_number;
    }

    template <typename T>
    void read(T persistent::* p) {
        seeprom_read(offset(p), &(this->*p), sizeof(T));
    }

    void read_all() {
        seeprom_read(0, &this->magic_number, sizeof(persistent));
    }

    void write_all() {
        seeprom_write(0, &this->magic_number, sizeof(persistent));
        seeprom_flush();
    }

    static inline ztimer_t m_write_delay = {
        .base = {},
        .callback = [](void*) { seeprom_flush(); },
        .arg = nullptr,
    };
};

template <typename T>
void persistent::write(T persistent::* p)
{
    seeprom_update(offset(p), &(this->*p), sizeof(T));
    ztimer_set(ZTIMER_MSEC, &m_write_delay, NVM_WRITE_DELAY_MS);
}
