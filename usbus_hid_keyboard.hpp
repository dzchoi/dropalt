#pragma once

#include "features.hpp"         // for KEYBOARD_REPORT_INTERVAL_MS
#include "hid_keycodes.hpp"
#include "usb_descriptor.hpp"
#include "usbus_hid_device.hpp"



class usbus_hid_keyboard_t: public usbus_hid_device_tl<AUTOMATIC_REPORTING> {
public:
    void on_reset();
    void on_suspend();
    void on_resume();

    // Note that led state can be changed only by the host, as we report KC_CAPSLOCK.
    uint8_t get_led_state() const { return m_keyboard_led_state; }

    uint8_t get_protocol() const { return m_keyboard_protocol; }

    void set_protocol(uint8_t protocol) { m_keyboard_protocol = protocol; }

protected:
    using usbus_hid_device_tl<AUTOMATIC_REPORTING>::usbus_hid_device_tl;

    void help_init(usbus_t* usbus, size_t epsize, uint8_t ep_interval_ms);

    uint8_t m_keyboard_led_state = 0;

    // Handle data packets received from the host.
    static void _hdlr_receive_data(usbus_hid_device_t* hid, uint8_t* data, size_t len);

    // 0 = Boot Protocol, 1 = Report Protocol (default)
    // Note that only the host (e.g. bios) can pull it down to boot protocol using
    // USB_HID_REQUEST_SET_PROTOCOL. Keyboard device cannot set it to 0 on its own.
    uint8_t m_keyboard_protocol = 1;

    bool help_bits_press(uint8_t& mods, uint8_t key);
    bool help_bits_release(uint8_t& mods, uint8_t key);
    bool help_skro_press(uint8_t keys[], uint8_t key);
    bool help_skro_release(uint8_t keys[], uint8_t key);
    bool help_nkro_press(uint8_t bits[], uint8_t key);
    bool help_nkro_release(uint8_t bits[], uint8_t key);
};

inline bool usbus_hid_keyboard_t::help_bits_press(uint8_t& bits, uint8_t key)
{
    const uint8_t mask = uint8_t(1) << (key & 7);
    if ( (bits & mask) != 0 )
        return false;

    changed = false;  // Disable _tmo_automatic_report() while updating.
    bits |= mask;
    changed = true;
    return true;
}

inline bool usbus_hid_keyboard_t::help_bits_release(uint8_t& bits, uint8_t key)
{
    const uint8_t mask = uint8_t(1) << (key & 7);
    if ( (bits & mask) == 0 )
        return false;

    changed = false;  // Disable _tmo_automatic_report() while updating.
    bits &= ~mask;
    changed = true;
    return true;
}



template <bool>
class usbus_hid_keyboard_tl;

constexpr bool SKRO = false;
constexpr bool NKRO = true;

template<>
class usbus_hid_keyboard_tl<SKRO>: public usbus_hid_keyboard_t {
public:
    static inline constexpr auto REPORT_DESC = array_of(SkroReportDescriptor);

    static constexpr size_t KEYBOARD_EPSIZE = SKRO_REPORT_SIZE;

    usbus_hid_keyboard_tl(usbus_t* usbus)
    : usbus_hid_keyboard_t(
        usbus, REPORT_DESC.data(), REPORT_DESC.size(), _hdlr_receive_data)
    {}

    void init(usbus_t* usbus) {
        help_init(usbus, KEYBOARD_EPSIZE, KEYBOARD_REPORT_INTERVAL_MS);
    }

    void _isr_fill_in_buf() { __builtin_memcpy(in_buf, m_report.raw, KEYBOARD_EPSIZE); }

    // Update the report that will be sent to the host automatically (no blocking).
    bool press(uint8_t key) {
        return key >= KC_LCTRL ?
            help_bits_press(m_report.mods, key) : help_skro_press(m_report.keys, key);
    }

    // Update the report that will be sent to the host automatically (no blocking).
    bool release(uint8_t key) {
        return key >= KC_LCTRL ?
            help_bits_release(m_report.mods, key) : help_skro_release(m_report.keys, key);
    }

private:
    union __attribute__((packed)) keyboard_report_t {
        uint8_t raw[SKRO_REPORT_SIZE];
        struct {
            uint8_t mods;
            uint8_t reserved;
            uint8_t keys[SKRO_KEYS_SIZE];
        };
    } m_report;
    static_assert( sizeof(keyboard_report_t) == KEYBOARD_EPSIZE );
};

template<>
class usbus_hid_keyboard_tl<NKRO>: public usbus_hid_keyboard_t {
public:
    static inline constexpr auto REPORT_DESC = array_of(NkroReportDescriptor);

    // The endpoint report size that we provided to the host in HID report descriptor.
    // Even so we should follow the current protocol; in Report protocol we report as we
    // provided, but in Boot protocol we report as 6KRO keyboard.
    static constexpr size_t KEYBOARD_EPSIZE = NKRO_REPORT_SIZE;

    usbus_hid_keyboard_tl(usbus_t* usbus)
    : usbus_hid_keyboard_t(
        usbus, REPORT_DESC.data(), REPORT_DESC.size(), _hdlr_receive_data)
    {}

    void init(usbus_t* usbus) {
        help_init(usbus, KEYBOARD_EPSIZE, KEYBOARD_REPORT_INTERVAL_MS);
    }

    void set_protocol(uint8_t protocol);

    void _isr_fill_in_buf() { __builtin_memcpy(in_buf, m_report.raw, KEYBOARD_EPSIZE); }

    bool press(uint8_t key) {
        return key >= KC_LCTRL ?
            help_bits_press(m_report.mods, key) : (this->*xkro_press)(key);
    }

    bool release(uint8_t key) {
        return key >= KC_LCTRL ?
            help_bits_release(m_report.mods, key) : (this->*xkro_release)(key);
    }

private:
    union __attribute__((packed)) keyboard_report_t {
        uint8_t raw[NKRO_REPORT_SIZE];
        struct {
            uint8_t mods;
            union {
                struct {                       // for Boot protocol
                    uint8_t reserved;
                    uint8_t keys[SKRO_KEYS_SIZE];
                };
                uint8_t bits[NKRO_KEYS_SIZE];  // for Report protocol
            };
        };
    } m_report;
    static_assert( sizeof(keyboard_report_t) == KEYBOARD_EPSIZE );

    // These pointers-to-members are changed according to the current protocol.
    bool (usbus_hid_keyboard_tl::*xkro_press)(uint8_t key) =
        &usbus_hid_keyboard_tl::nkro_press;
    bool (usbus_hid_keyboard_tl::*xkro_release)(uint8_t key) =
        &usbus_hid_keyboard_tl::nkro_release;

    bool skro_press(uint8_t key) { return help_skro_press(m_report.keys, key); }
    bool skro_release(uint8_t key) { return help_skro_release(m_report.keys, key); }
    bool nkro_press(uint8_t key) { return help_nkro_press(m_report.bits, key); }
    bool nkro_release(uint8_t key) { return help_nkro_release(m_report.bits, key); }
};
