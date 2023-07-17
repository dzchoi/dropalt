#pragma once

#include <atomic>               // for std::atomic<>
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
    // They return false if USB is not accessible.
    //
    // Beware: these methods are supposed to be called from only one thread
    //  (keymap_thread). Otherwise, we need to serialize the calls of the methods by
    //  using e.g. events on usb_thread, in order to ensure that report is not updated
    //  simultaneously. (However, we cannot use mutex in the event handler as it will
    //  acquire the mutex in the context of usb_thread and interrupt, which can possibly
    //  cause usb_thread that executes the event handler being stuck.)
    bool report_press(uint8_t keycode);

    bool report_release(uint8_t keycode);

    // Called (in usb_thread context) when the report is received by the host.
    void on_transfer_complete();

    // Called (in ISR context) when the report is not received, hence lost, by the host.
    void isr_on_transfer_timeout();

protected:
    using usbus_hid_device_ext_t::usbus_hid_device_ext_t;

    void help_usb_init(usbus_t* usbus, size_t epsize, uint8_t ep_interval_ms);

    uint8_t m_led_lamp_state = 0;

    // 0 = Boot protocol, 1 = Report protocol (default)
    // Note that only the host (e.g. bios) can pull it down to Boot protocol, using
    // USB_HID_REQUEST_SET_PROTOCOL; Keyboard device cannot set it to 0 on its own.
    uint8_t m_keyboard_protocol = 1;

    // Indicator if the report is submitted to the host
    // (0 = nor submitted, 1 = submitted, 2+ = submitted and updated further)
    std::atomic<uint8_t> m_report_updated = 0;

    // Key press that has been updated to report but not submitted.
    std::atomic<uint8_t> m_press_yet_to_submit = KC_NO;

    // USB is not accessible immediately after USBUS_EVENT_USB_RESUME, or in the case of
    // Linux even after usbus->state is changed to USBUS_STATE_CONFIGURED. We give a
    // little delay to allow keymap_thread to send key events.
    std::atomic<bool> m_is_usb_accessible = false;

    ztimer_t m_delay_usb_accessible = {
        .base = {},
        .callback = _tmo_usb_accessible,
        .arg = this,
    };
    static void _tmo_usb_accessible(void* arg);

    // Submit the current report to the host.
    void submit_report();

    // Used by submit_report() to copy the report to in_buf.
    virtual void fill_in_buf() =0;

    // Update the report and return true if successful. This private method is used by
    // report_press/release().
    virtual bool update_report(uint8_t keycode, bool is_press) =0;

    bool help_update_bits(uint8_t& bits, uint8_t keycode, bool is_press);
    bool help_update_skro_report(uint8_t keys[], uint8_t keycode, bool is_press);
    bool help_update_nkro_report(uint8_t bits[], uint8_t keycode, bool is_press);

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

    void usb_init(usbus_t* usbus) {
        help_usb_init(usbus, KEYBOARD_EPSIZE, KEYBOARD_REPORT_INTERVAL_MS);
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

    bool update_report(uint8_t keycode, bool is_press) {
        return keycode >= KC_LCTRL
            ? help_update_bits(m_report.mods, keycode, is_press)
            : help_update_skro_report(m_report.keys, keycode, is_press);
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

    void usb_init(usbus_t* usbus) {
        help_usb_init(usbus, KEYBOARD_EPSIZE, KEYBOARD_REPORT_INTERVAL_MS);
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

    bool update_report(uint8_t keycode, bool is_press) {
        return keycode >= KC_LCTRL
            ? help_update_bits(m_report.mods, keycode, is_press)
            : (this->*xkro_report_key)(keycode, is_press);
    }

    // These pointers-to-methods are changed according to the current protocol.
    bool (usbus_hid_keyboard_tl::*xkro_report_key)(uint8_t keycode, bool is_press) =
        &usbus_hid_keyboard_tl::nkro_report_key;

    bool skro_report_key(uint8_t keycode, bool is_press) {
        return help_update_skro_report(m_report.keys, keycode, is_press);
    }

    bool nkro_report_key(uint8_t keycode, bool is_press) {
        return help_update_nkro_report(m_report.bits, keycode, is_press);
    }
};
