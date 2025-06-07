#pragma once

#include "mutex.h"              // for mutex_t

#include "config.hpp"           // for KEYBOARD_REPORT_INTERVAL_MS
#include "event_ext.hpp"        // for event_ext_t<>
#include "hid_keycodes.hpp"     // for KC_LCTRL, KC_NO
#include "usb_descriptor.hpp"
#include "usbus_hid_device.hpp"

// Note that physical key press/release events are generated from matrix_thread, sent to
// keymap_thread, converted into USB keycodes, and finally reported to usb_thread through
// usbus_hid_keyboard.



class key_event_queue_t {
public:
    key_event_queue_t(mutex_t& mutex): m_not_full(mutex) {}

    struct key_event_t { uint8_t keycode; bool is_press; };
    static_assert( sizeof(key_event_t) == 2 );

    // These methods are NOT thread-safe. It is the caller's responsibility to ensure
    // thread safety. push() is invoked by report_event() from the client thread
    // (keymap_thread) with interrupts disabled. The remaining methods are exclusively
    // executed by usb_thread, which has the highest priority, ensuring they run without
    // preemption.

    void push(key_event_t event, bool wait_if_full =false);
    bool pop();
    void clear();

    bool not_empty() const { return m_begin != m_end; }
    bool not_full() const { return (m_end - m_begin) < QUEUE_SIZE; }
    bool peek(key_event_t& event) const;

private:
    // The usbus_hid_device_t::in_lock serves as m_not_full, preventing the queue from
    // overflowing by locking when full. It does not protect simultaneous access to
    // usbus_hid_device_t::in_buf, which is inherently safeguarded since submit_report()
    // is exclusively invoked from usb_thread, not from other threads or interrupts.
    mutex_t& m_not_full;  // The mutex must be initialized already.

    static constexpr size_t QUEUE_SIZE = 32;  // must be a power of two.
    static_assert( (QUEUE_SIZE > 0) && ((QUEUE_SIZE & (QUEUE_SIZE - 1)) == 0) );

    key_event_t m_events[QUEUE_SIZE];

    size_t m_begin = 0;
    size_t m_end = 0;
};



class usbus_hid_keyboard_t: public usbus_hid_device_ext_t {
public:
    void on_reset() override;
    void on_suspend() override;
    void on_resume() override;

    // Note that lamp state can be changed only by the host, in response to KC_CAPSLOCK.
    uint8_t get_lamp_state() const { return m_led_lamp_state; }

    uint8_t get_protocol() const override { return m_keyboard_protocol; }

    // 6KRO is utilized regardless of whether protocol is set to 0 or 1.
    void set_protocol(uint8_t protocol) override { m_keyboard_protocol = protocol; }

    // These two user-facing methods register key press/release events to the host. They
    // are thread-safe and remain non-blocking as long as m_key_event_queue is not full.
    // If the queue is full and USB is accessible, they block the caller thread
    // (keymap_thread). However, if USB is not accessible, they do not block; instead,
    // they discard the oldest events when the queue is full or clear the entire queue if
    // no key event has occurred for SUSPENDED_KEY_EVENT_LIFETIME_MS.
    // There is no need to introduce an explicit delay between consecutive calls, as this
    // is handled internally. You can call report_press() and report_release() back-to-
    // back for the same key.
    void report_press(uint8_t keycode) { report_event(keycode, true); }
    void report_release(uint8_t keycode) { report_event(keycode, false); }

protected:
    using usbus_hid_device_ext_t::usbus_hid_device_ext_t;

    void help_usb_init(usbus_t* usbus, size_t epsize, uint8_t ep_interval_ms);

    uint8_t m_led_lamp_state = 0;

    // 0 = Boot protocol, 1 = Report protocol (default)
    // Note that only the host (e.g. bios) can pull it down to Boot protocol, using
    // USB_HID_REQUEST_SET_PROTOCOL. Keyboard device cannot set it to 0 on its own.
    uint8_t m_keyboard_protocol = 1;

    // On a Linux host, USB is not immediately accessible after usbus->state transitions
    // to USBUS_STATE_CONFIGURED (i.e. usbus_is_active() becomes true). A short delay is
    // introduced before using USB to ensure stability.
    bool m_is_usb_accessible = false;

    ztimer_t m_delay_usb_accessible = {
        .callback = _tmo_usb_accessible,
        .arg = this,
    };
    static void _tmo_usb_accessible(void* arg);

    // Indicator if the report has been submitted to the host in the current packet frame.
    // (0 = not updated nor submitted, 1 = submitted, 2+ = submitted but updated further)
    uint8_t m_report_updated = 0;

    // Key press that has been updated in the report but not submitted.
    uint8_t m_press_yet_to_submit = KC_NO;

    // Queue to buffer the key events received from the client thread as necessary.
    key_event_queue_t m_key_event_queue { in_lock };

    // Try to report a key event in the current packet frame, return false if not
    // possible.
    bool try_report_event(uint8_t keycode, bool is_press);

    // Report a key event in the current packet frame if possible. Otherwise put it in
    // the event queue so that it can be reported in next packet frame(s).
    void report_event(uint8_t keycode, bool is_press);

    // Timer to wait for SUSPENDED_KEY_EVENT_LIFETIME_MS. The event queue is cleared via
    // an event (m_event_clear_queue) from the timer handler. If the queue were cleared
    // directly within the timer handler (on interrupt context), all related functions
    // would need to be wrapped with irq_disable/enable() to ensure thread safety.
    ztimer_t m_timer_clear_queue = {
        .callback = [](void* arg) {
            usbus_hid_keyboard_t* const hidx = static_cast<usbus_hid_keyboard_t*>(arg);
            usbus_event_post(hidx->usbus, &hidx->m_event_clear_queue);
        },
        .arg = this,
    };
    event_ext_t<usbus_hid_keyboard_t*> m_event_clear_queue = {
        nullptr,  // .list_node
        [](event_t* pevent) {  // .handler
            usbus_hid_keyboard_t* const hidx =
                static_cast<event_ext_t<usbus_hid_keyboard_t*>*>(pevent)->arg;
            hidx->m_key_event_queue.clear();
        },
        this  // .arg
    };

    // Submit the current report to the host.
    void submit_report();

    // Called when the current packet frame is delivered to the host successfully, or
    // when it failed, or when USB becomes accessible.
    void on_transfer_complete(bool was_successful) override;

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

    void usb_init(usbus_t* usbus) override {
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

    void fill_in_buf() override {
        __builtin_memcpy(in_buf, m_report.raw, KEYBOARD_EPSIZE);
    }

    bool update_report(uint8_t keycode, bool is_press) override {
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

    void usb_init(usbus_t* usbus) override {
        help_usb_init(usbus, KEYBOARD_EPSIZE, KEYBOARD_REPORT_INTERVAL_MS);
    }

    void set_protocol(uint8_t protocol) override;

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

    void fill_in_buf() override {
        __builtin_memcpy(in_buf, m_report.raw, KEYBOARD_EPSIZE);
    }

    bool update_report(uint8_t keycode, bool is_press) override {
        return keycode >= KC_LCTRL
            ? help_update_bits(m_report.mods, keycode, is_press)
            : (this->*xkro_report_key)(keycode, is_press);
    }

    // This pointer-to-method is changed according to the current protocol.
    bool (usbus_hid_keyboard_tl::*xkro_report_key)(uint8_t keycode, bool is_press) =
        &usbus_hid_keyboard_tl::nkro_report_key;

    bool skro_report_key(uint8_t keycode, bool is_press) {
        return help_update_skro_report(m_report.keys, keycode, is_press);
    }

    bool nkro_report_key(uint8_t keycode, bool is_press) {
        return help_update_nkro_report(m_report.bits, keycode, is_press);
    }
};
