#pragma once

#include "features.hpp"         // for KEYBOARD_REPORT_INTERVAL_MS
#include "hid_keycodes.hpp"
#include "usb_descriptor.hpp"
#include "usbus_hid_device.hpp"
#include "xtimer_wrapper.hpp"



class usbus_hid_keyboard_t: public usbus_hid_device_tl<AUTOMATIC_REPORTING> {
public:
    void on_reset();
    void on_suspend();
    void on_resume();

    // Note that led state can be changed only by the host, as we report KC_CAPSLOCK.
    uint8_t get_led_state() const { return m_keyboard_led_state; }

    uint8_t get_protocol() const { return m_keyboard_protocol; }

    void set_protocol(uint8_t protocol) { m_keyboard_protocol = protocol; }

    virtual bool key_report(uint8_t keycode, bool pressed) =0;

    bool key_press(uint8_t keycode) { return key_report(keycode, true); }
    bool key_release(uint8_t keycode) { return key_report(keycode, false); }
    // Todo: Delayed release should be handled through list of pressed keys.
    bool key_tap(uint8_t keycode);

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

    bool help_update_report_bits(uint8_t& bits, uint8_t keycode, bool pressed);
    bool help_skro_key_report(uint8_t keys[], uint8_t keycode, bool pressed);
    bool help_nkro_key_report(uint8_t bits[], uint8_t keycode, bool pressed);

    uint8_t key_in_press;  // No need to initialize.
    static constexpr uint32_t TAPPING_DELAY_RELEASE_US = 20 *US_PER_MS;
    xtimer_onetime_callback_t<usbus_hid_keyboard_t*> tap_timer {
        TAPPING_DELAY_RELEASE_US, _tmo_key_delayed_release, this };
    static void _tmo_key_delayed_release(usbus_hid_keyboard_t* that) {
        that->key_release(that->key_in_press);
    }
};

inline bool usbus_hid_keyboard_t::help_update_report_bits(
    uint8_t& bits, uint8_t keycode, bool pressed)
{
    const uint8_t mask = uint8_t(1) << (keycode & 7);
    if ( ((bits & mask) != 0) == pressed )
        return false;

    changed = false;  // Disable _tmo_automatic_report() while updating.
    if ( pressed )
        bits |= mask;
    else
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
    bool key_report(uint8_t keycode, bool pressed) {
        return keycode >= ::keycode(KC_LCTRL)
            ? help_update_report_bits(m_report.mods, keycode, pressed)
            : help_skro_key_report(m_report.keys, keycode, pressed);
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

    bool key_report(uint8_t keycode, bool pressed) {
        return keycode >= ::keycode(KC_LCTRL)
            ? help_update_report_bits(m_report.mods, keycode, pressed)
            : (this->*xkro_key_report)(keycode, pressed);
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
    bool (usbus_hid_keyboard_tl::*xkro_key_report)(uint8_t keycode, bool pressed) =
        &usbus_hid_keyboard_tl::nkro_key_report;

    bool skro_key_report(uint8_t keycode, bool pressed) {
        return help_skro_key_report(m_report.keys, keycode, pressed);
    }

    bool nkro_key_report(uint8_t keycode, bool pressed) {
        return help_nkro_key_report(m_report.bits, keycode, pressed);
    }
};
