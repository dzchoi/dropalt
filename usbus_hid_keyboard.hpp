#pragma once

#include <mutex.h>

#include "event_ext.hpp"        // for event_ext_t
#include "features.hpp"         // for KEYBOARD_REPORT_INTERVAL_MS
#include "hid_keycodes.hpp"     // for KC_LCTRL, KC_NO
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

    // These user-facing methods register key presses/releases by triggering
    // corresponding events to usb_thread. Note that they are not to be called from
    // interrupt context. They use the private method report_key() to send report to the
    // host, but after blocking if necesssay, in order to ensure that no two presses are
    // made in the same report (for NKRO protocol in particular) and that a press and
    // its immediate release are not reported at the same time.
    void report_press(uint8_t keycode);

    void report_release(uint8_t keycode);

    // This code needs fixing _usbus_thread() to handle multiple events on event queue,
    // as an event can be pushed from interrupt while usbus (i.e. usb_thread) processing
    // another event in the queue.
    void _isr_report_done() { usbus_event_post(usbus, &m_event_report_done); }

protected:
    usbus_hid_keyboard_t(usbus_t* usbus,
        const uint8_t* report_desc, size_t report_desc_size, usbus_hid_cb_t cb_receive_data);

    void help_init(usbus_t* usbus, size_t epsize, uint8_t ep_interval_ms);

    uint8_t m_keyboard_led_state = 0;

    // Handle data packets received from the host.
    static void _hdlr_receive_data(usbus_hid_device_t* hid, uint8_t* data, size_t len);

    // 0 = Boot Protocol, 1 = Report Protocol (default)
    // Note that only the host (e.g. bios) can pull it down to boot protocol using
    // USB_HID_REQUEST_SET_PROTOCOL. Keyboard device cannot set it to 0 on its own.
    uint8_t m_keyboard_protocol = 1;

    mutex_t m_lock_send;
    mutex_t m_lock_release;
    uint8_t m_key_being_reported = KC_NO;

    // Event used by report_press/release()
    event_ext_t<usbus_hid_keyboard_t*, uint8_t> m_event_report;

    static void _hdlr_report_press(event_t* event);
    static void _hdlr_report_release(event_t* event);

    // Event triggered when report is sent to the host
    event_ext_t<usbus_hid_keyboard_t*> m_event_report_done;

    static void _hdlr_report_done(event_t* event);

    // Update the report that will be sent to the host automatically (non-blocking).
    virtual bool report_key(uint8_t keycode, bool pressed) =0;

    bool help_report_bits(uint8_t& bits, uint8_t keycode, bool pressed);
    bool help_skro_report_key(uint8_t keys[], uint8_t keycode, bool pressed);
    bool help_nkro_report_key(uint8_t bits[], uint8_t keycode, bool pressed);
};



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
