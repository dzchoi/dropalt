#pragma once

#include <stdint.h>             // for uint*_t
#include "seeprom.h"
#include "usb2422.h"            // for USB_PORT_1
#include "ztimer.h"             // for ztimer_t

#include <cstdarg>              // for va_list
#include "color.hpp"            // for hsv_t
#include "features.hpp"         // for NVM_MAGIC_NUMBER, NVM_WRITE_DELAY_MS



class __attribute__((packed)) persistent {
public:
    // Data members with their default values, with which persistent::m_default will be
    // initialized.

    uint32_t magic_number = NVM_MAGIC_NUMBER;
    uint16_t layout_version = 1u + (4u << 8);  // major version and minor version

    uint16_t occupied = sizeof(persistent);

    // Todo: Effect instead of a color?
    hsv_t led_color = hsv_t{ ORANGE, 255, 255 };

    uint8_t last_host_port = USB_PORT_1;

    // Log strings are gathered as one big null-terminated string into the rest of NVM,
    // separated by a newline. The total size of occupied NVM is kept track of by
    // `occupied`, not including the null character. So, occupied < seeprom_size() in
    // normal cases, but we will have occupied == seeprom_size() if logs are truncated
    // to fit in seeprom_size() (the last byte of Seeprom will still have '\0' though).

public:
    static void init();

    static persistent& obj() {
        seeprom_unbuffered();
        return *reinterpret_cast<persistent*>(seeprom_addr(0));
    }

    // There are two ways to write into NVM, e.g. `persistent::obj().last_host_port = 2`
    // and `persistent::write(&persistent::last_host_port, 2)`. The first uses unbuffered
    // mode while the second uses buffered mode. In buffered mode the actual write into
    // NVM will be delayed by NVM_WRITE_DELAY_MS. See the comment in seeprom.h.
    template <typename T>
    static void write(T persistent::* p, const T& value);

    // Likewise, we can read NVM using either persistent::obj().last_host_port or
    // persistent::read(&persistent::last_host_port, &value), which have no difference
    // though.
    template <typename T>
    static void read(T persistent::* p, T* pvalue) {
        seeprom_read(offset(p), pvalue, sizeof(T));
    }

    static bool has_log() { return obj().occupied > sizeof(persistent); }

    static void clear_log() { obj().occupied = sizeof(persistent); }

    static const char* get_log() {
        return static_cast<const char*>(seeprom_addr(sizeof(persistent)));
    }

    static void add_log(const char* format, va_list vlist);

private:
    template <typename T>
    static intptr_t offset(T persistent::* p) {
        return (uint8_t*)&(m_default.*p) - (uint8_t*)&m_default.magic_number;
    }

    // Reset NVM to their default values.
    static void reset_all();

    static const persistent m_default;

    static inline ztimer_t m_write_delay = {
        .base = {},
        .callback = [](void*) { seeprom_flush(); },
        .arg = nullptr,
    };
};

template <typename T>
void persistent::write(T persistent::* p, const T& value)
{
    seeprom_buffered();
    // By using seeprom_update() instead of seeprom_write() we write the value only when
    // the value is changed.
    seeprom_update(offset(p), &value, sizeof(T));
    ztimer_set(ZTIMER_MSEC, &m_write_delay, NVM_WRITE_DELAY_MS);
}
