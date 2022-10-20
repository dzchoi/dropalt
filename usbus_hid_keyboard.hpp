#pragma once

#include "xtimer.h"

#include <vector>               // for std::vector<> from Newlib
#include "event_ext.hpp"        // for event_ext_t
#include "features.hpp"         // for KEYBOARD_REPORT_INTERVAL_MS
#include "hid_keycodes.hpp"     // for KC_LCTRL
#include "usb_descriptor.hpp"
#include "usbus_hid_device.hpp"

// Note that there are two types of key press/release events, those done physically and
// those done reportedly (to the host). Physical events are received by matrix_thread,
// mapped by keymap_thread, and finally reported to usbus_hid_keyboard (i.e. usb_thread).
// The "keys" here in usbus_hid_keyboard refer to the type being reported.



class usbus_hid_keyboard_t: public usbus_hid_device_tl<AUTOMATIC_REPORTING> {
public:
    void on_reset();
    void on_suspend();
    void on_resume();

    // Note that led state can be changed only by the host, as we report KC_CAPSLOCK.
    uint8_t get_led_state() const { return m_keyboard_led_state; }

    uint8_t get_protocol() const { return m_keyboard_protocol; }

    void set_protocol(uint8_t protocol) { m_keyboard_protocol = protocol; }

    // These methods register key presses/taps/releases by sending corresponding events
    // to usb_thread. Note that they are not to be called from interrupt context.
    // (Using the same event instance with only different arguments is usually prone to
    // lose any previous events that are overwritten by a later one. In this case,
    // however, as usb_thread has the highest priority and the events cannot be sent
    // from interrupts, there can be at maximum only one event being handled at any
    // moment.)
    void report_press(uint8_t keycode) {
        m_event_report.handler = _hdlr_report_press;
        m_event_report.arg1 = keycode;
        usbus_event_post(usbus, &m_event_report);
    }

    void report_tap(uint8_t keycode) {
        m_event_report.handler = _hdlr_report_tap;
        m_event_report.arg1 = keycode;
        usbus_event_post(usbus, &m_event_report);
    }

    void report_release(uint8_t keycode) {
        m_event_report.handler = _hdlr_report_release;
        m_event_report.arg1 = keycode;
        usbus_event_post(usbus, &m_event_report);
    }

protected:
    usbus_hid_keyboard_t(usbus_t* usbus,
        const uint8_t* report_desc, size_t report_desc_size, usbus_hid_cb_t cb_receive_data)
    : usbus_hid_device_tl(usbus, report_desc, report_desc_size, cb_receive_data)
    {
        // Initialize the fixed part of m_event_report.
        m_event_report.arg0 = this;
        // Initial capacity of m_reported_keys. It will be increased as needed.
        m_reported_keys.reserve(SKRO_KEYS_SIZE);
    }

    void help_init(usbus_t* usbus, size_t epsize, uint8_t ep_interval_ms);

    uint8_t m_keyboard_led_state = 0;

    // Handle data packets received from the host.
    static void _hdlr_receive_data(usbus_hid_device_t* hid, uint8_t* data, size_t len);

    // 0 = Boot Protocol, 1 = Report Protocol (default)
    // Note that only the host (e.g. bios) can pull it down to boot protocol using
    // USB_HID_REQUEST_SET_PROTOCOL. Keyboard device cannot set it to 0 on its own.
    uint8_t m_keyboard_protocol = 1;

    // *report_press/tap/release methods take care of the events not to conflict (e.g.
    // consecutive presses without a release in between) and use report_key() to finally
    // compose the report and send it, whereas *report_key/bits methods only compose
    // the report according to SKRO or NKRO protocol.

    bool help_report_press(uint8_t keycode, bool is_tapping =false);
    bool help_report_release(uint8_t keycode);

    // Update the report that will be sent to the host automatically (non-blocking).
    virtual bool report_key(uint8_t keycode, bool pressed) =0;

    bool help_report_bits(uint8_t& bits, uint8_t keycode, bool pressed);
    bool help_skro_report_key(uint8_t keys[], uint8_t keycode, bool pressed);
    bool help_nkro_report_key(uint8_t bits[], uint8_t keycode, bool pressed);

    struct reported_key_t {
        reported_key_t(uint8_t keycode): code(keycode), ref_count(1) {
            tapping_timer.callback = _tmo_delayed_release;
            tapping_timer.arg = this;
        }

        // Timer that starts when pressed using report_tap(). On expire, the key gets
        // released.
        // Todo: What happens to tapping_timers that happen to be running when the vector
        //  relocates the elements? Using a list instead of vector is preferred?
        xtimer_t tapping_timer;
        uint8_t code;
        uint8_t ref_count;

        static void _tmo_delayed_release(void* arg);
    };

    std::vector<reported_key_t> m_reported_keys;

    event_ext_t<usbus_hid_keyboard_t*, uint8_t> m_event_report;

    static void _hdlr_report_press(event_t* event);
    static void _hdlr_report_tap(event_t* event);
    static void _hdlr_report_release(event_t* event);
};

inline bool usbus_hid_keyboard_t::help_report_bits(
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
    static constexpr auto REPORT_DESC = array_of(SkroReportDescriptor);

    static constexpr size_t KEYBOARD_EPSIZE = SKRO_REPORT_SIZE;

    usbus_hid_keyboard_tl(usbus_t* usbus)
    : usbus_hid_keyboard_t(
        usbus, REPORT_DESC.data(), REPORT_DESC.size(), _hdlr_receive_data)
    {}

    void init(usbus_t* usbus) {
        help_init(usbus, KEYBOARD_EPSIZE, KEYBOARD_REPORT_INTERVAL_MS);
    }

    void _isr_fill_in_buf() { __builtin_memcpy(in_buf, m_report.raw, KEYBOARD_EPSIZE); }

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

    bool report_key(uint8_t keycode, bool pressed) {
        return keycode >= KC_LCTRL
            ? help_report_bits(m_report.mods, keycode, pressed)
            : help_skro_report_key(m_report.keys, keycode, pressed);
    }
};

template<>
class usbus_hid_keyboard_tl<NKRO>: public usbus_hid_keyboard_t {
public:
    static constexpr auto REPORT_DESC = array_of(NkroReportDescriptor);

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
    bool (usbus_hid_keyboard_tl::*xkro_report_key)(uint8_t keycode, bool pressed) =
        &usbus_hid_keyboard_tl::nkro_report_key;

    bool skro_report_key(uint8_t keycode, bool pressed) {
        return help_skro_report_key(m_report.keys, keycode, pressed);
    }

    bool nkro_report_key(uint8_t keycode, bool pressed) {
        return help_nkro_report_key(m_report.bits, keycode, pressed);
    }

    bool report_key(uint8_t keycode, bool pressed) {
        return keycode >= KC_LCTRL
            ? help_report_bits(m_report.mods, keycode, pressed)
            : (this->*xkro_report_key)(keycode, pressed);
    }
};
