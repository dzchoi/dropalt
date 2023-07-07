#pragma once

#include <atomic>
#include "features.hpp"         // for KEYBOARD_REPORT_INTERVAL_MS
#include "hid_keycodes.hpp"     // for KC_LCTRL, KC_NO
#include "usb_descriptor.hpp"
#include "usbus_hid_device.hpp"

// Note that there are two types of key press/release events, those done physically and
// those done reportedly (to the host). Physical events are received by matrix_thread,
// mapped by keymap_thread, and finally reported to usbus_hid_keyboard (i.e. usb_thread).
// The "keys" here in usbus_hid_keyboard refer to the type being reported.



class usbus_hid_keyboard_t: public usbus_hid_device_ext_t {
public:
    void on_reset();
    void on_suspend();
    void on_resume();

    // Note that lamp state can be changed only by the host, in response to KC_CAPSLOCK.
    uint8_t get_lamp_state() const { return m_led_lamp_state; }

    uint8_t get_protocol() const { return m_keyboard_protocol; }

    void set_protocol(uint8_t protocol) { m_keyboard_protocol = protocol; }

    // These two user-facing methods register key press/release to the host. Report is
    // made in the context of calling thread, before submitting it to the host in the
    // usb_thread context. While making the report it can block the calling thread to
    // ensure:
    //  - no two presses are made in the same report (for NKRO protocol especially), and
    //  - a press and its release are not reported in the same report.
    //
    // Beware: they are assumed to be called from only one thread (keymap_thread).
    //  Otherwise, we need to serialize the calling of the methods by using e.g. events
    //  on usb_thread, in order to ensure that report is not accessed simultaneously.
    //  (We cannot use mutex in this case as we need to acquire the mutex in the context
    //  of usb_thread and interrupt.)
    void report_press(uint8_t keycode);

    void report_release(uint8_t keycode);

    // Called (in usb_thread context) when the report is received by the host.
    void on_transfer_complete();

    // Called (in ISR context) when the report is not received, hence lost, by the host.
    void isr_on_transfer_timeout();

protected:
    using usbus_hid_device_ext_t::usbus_hid_device_ext_t;

    void help_init(usbus_t* usbus, size_t epsize, uint8_t ep_interval_ms);

    uint8_t m_led_lamp_state = 0;

    // 0 = Boot protocol, 1 = Report protocol (default)
    // Note that only the host (e.g. bios) can pull it down to Boot protocol, using
    // USB_HID_REQUEST_SET_PROTOCOL; Keyboard device cannot set it to 0 on its own.
    uint8_t m_keyboard_protocol = 1;

    // Indicator if the report is submitted to the host
    // (0 = nor submitted, 1 = submitted, 2+ = submitted and updated further)
    std::atomic<uint8_t> m_report_updated = 0;

    // Press that is updated to report but not submitted.
    std::atomic<uint8_t> m_press_yet_to_submit = KC_NO;

    // Submit the current report to the host.
    void submit();

    // Used by submit() to copy the report to in_buf.
    virtual void fill_in_buf() =0;

    // Update the report, used by report_press() and report_release().
    virtual bool report_key(uint8_t keycode, bool pressed) =0;

    bool help_report_bits(uint8_t& bits, uint8_t keycode, bool pressed);
    bool help_skro_report_key(uint8_t keys[], uint8_t keycode, bool pressed);
    bool help_nkro_report_key(uint8_t bits[], uint8_t keycode, bool pressed);

    // Handle (in usb_thread context) the data packet received from the host.
    static void _hdlr_receive_data(usbus_hid_device_t* hid, uint8_t* data, size_t len);
};



template <bool>
class usbus_hid_keyboard_tl;

constexpr bool SKRO = false;
constexpr bool NKRO = true;

template<>
class usbus_hid_keyboard_tl<SKRO>: public usbus_hid_keyboard_t {
public:
    static constexpr size_t KEYBOARD_EPSIZE = SKRO_REPORT_SIZE;
    static inline const auto report_desc = array_of(SkroReportDescriptor);

    usbus_hid_keyboard_tl(usbus_t* usbus)
    : usbus_hid_keyboard_t(
        usbus, report_desc.data(), report_desc.size(), _hdlr_receive_data)
    {}

    void init(usbus_t* usbus) {
        help_init(usbus, KEYBOARD_EPSIZE, KEYBOARD_REPORT_INTERVAL_MS);
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

    void fill_in_buf() { __builtin_memcpy(in_buf, m_report.raw, KEYBOARD_EPSIZE); }

    bool report_key(uint8_t keycode, bool pressed) {
        return keycode >= KC_LCTRL
            ? help_report_bits(m_report.mods, keycode, pressed)
            : help_skro_report_key(m_report.keys, keycode, pressed);
    }
};

template<>
class usbus_hid_keyboard_tl<NKRO>: public usbus_hid_keyboard_t {
public:
    // The endpoint report size that we provide to the host in HID report descriptor.
    // Even so we should follow the current protocol; in Report protocol we report as we
    // provided, but in Boot protocol we report as 6KRO keyboard.
    static constexpr size_t KEYBOARD_EPSIZE = NKRO_REPORT_SIZE;
    static inline const auto report_desc = array_of(NkroReportDescriptor);

    usbus_hid_keyboard_tl(usbus_t* usbus)
    : usbus_hid_keyboard_t(
        usbus, report_desc.data(), report_desc.size(), _hdlr_receive_data)
    {}

    void init(usbus_t* usbus) {
        help_init(usbus, KEYBOARD_EPSIZE, KEYBOARD_REPORT_INTERVAL_MS);
    }

    void set_protocol(uint8_t protocol);

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

    void fill_in_buf() { __builtin_memcpy(in_buf, m_report.raw, KEYBOARD_EPSIZE); }

    bool report_key(uint8_t keycode, bool pressed) {
        return keycode >= KC_LCTRL
            ? help_report_bits(m_report.mods, keycode, pressed)
            : (this->*xkro_report_key)(keycode, pressed);
    }

    // These pointers-to-methods are changed according to the current protocol.
    bool (usbus_hid_keyboard_tl::*xkro_report_key)(uint8_t keycode, bool pressed) =
        &usbus_hid_keyboard_tl::nkro_report_key;

    bool skro_report_key(uint8_t keycode, bool pressed) {
        return help_skro_report_key(m_report.keys, keycode, pressed);
    }

    bool nkro_report_key(uint8_t keycode, bool pressed) {
        return help_nkro_report_key(m_report.bits, keycode, pressed);
    }
};
