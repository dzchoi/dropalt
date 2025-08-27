#pragma once

#include "mutex.h"              // for mutex_t

#include "config.hpp"           // for KEYBOARD_REPORT_INTERVAL_MS
#include "event_ext.hpp"        // for event_ext_t<>
#include "hid_keycodes.hpp"     // for KC_LCTRL, KC_NO
#include "usb_descriptor.hpp"
#include "usbus_hid_device.hpp"



class key_event_queue_t {
public:
    key_event_queue_t(mutex_t& mutex): m_not_full(mutex) {}

    struct key_event_t { uint8_t keycode; bool is_press; };
    static_assert( sizeof(key_event_t) == 2 );

    // These methods are NOT thread-safe. It is the caller's responsibility to ensure
    // safe access and synchronization.
    // - push() is invoked by report_event() from the client thread (main_thread),
    //   with interrupts disabled.
    // - The remaining methods are used exclusively by usb_thread, which runs at the
    //   highest priority and is not preempted.

    void push(key_event_t event, bool wait_if_full =false);
    bool pop();
    void clear();

    bool not_empty() const { return m_begin != m_end; }
    bool not_full() const { return (m_end - m_begin) < QUEUE_SIZE; }
    bool peek(key_event_t& event) const;

private:
    // The original usbus_hid_device_t::in_lock is repurposed as m_not_full, used to
    // prevent queue overflow by locking when full. It no longer guards concurrent
    // access to usbus_hid_device_t::in_buf, as submit_report() is exclusively called
    // from usb_thread (not from any lower-priority thread or from interrupts), ensuring
    // inherent thread safety without additional locking.
    mutex_t& m_not_full;  // must be initialized already.

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

    uint8_t get_protocol() const override { return m_keyboard_protocol; }

    // 6KRO is used by default, regardless of whether the protocol is set to Boot (0) or
    // Report (1).
    void set_protocol(uint8_t protocol) override { m_keyboard_protocol = protocol; }

    // These two user-facing methods register key press/release events to the host. They
    // are thread-safe, and non-blocking as long as the key event queue is not full. If
    // the queue is full and USB is accessible, the caller thread (main_thread) will
    // block until space becomes available. If USB is not accessible, the methods remain
    // non-blocking and silently discard the oldest events.
    // Note: No explicit delay between consecutive calls is required — this is handled
    // internally. You can safely call report_press() and report_release() back-to-back
    // for the same key.
    void report_press(uint8_t keycode) { report_event(keycode, true); }
    void report_release(uint8_t keycode) { report_event(keycode, false); }

protected:
    using usbus_hid_device_ext_t::usbus_hid_device_ext_t;

    void help_usb_init(usbus_t* usbus, size_t epsize, uint8_t ep_interval_ms);

    // The device defaults to Report protocol (1), but will switch to Boot protocol (0)
    // if explicitly instructed by the host (e.g. BIOS) via a SET_PROTOCOL request.
    uint8_t m_keyboard_protocol = 1;

    // On Linux, the input subsystem (evdev) may not yet be ready to deliver HID reports
    // to user space, even though the USB subsystem has successfully received and
    // acknowledged them. This results in USB being inaccessible immediately after
    // usbus->state transitions to USBUS_STATE_CONFIGURED (or when usbus_is_active()
    // returns true). A flag introduces a brief delay to block event reporting until the
    // system stabilizes.
    bool m_is_usb_accessible = false;

    ztimer_t m_timer_resume_settle = { .callback = _tmo_resume_settle, .arg = this };
    static void _tmo_resume_settle(void* arg);

    // Indicates the report's submission status during the current packet frame:
    // - 0: Report has not been updated or submitted.
    // - 1: Report has been submitted without further changes.
    // - 2+: Report was submitted, then updated again (requires re-submission).
    uint8_t m_report_updated = 0;

    // Key press that has been added in the report but not submitted to the host.
    uint8_t m_press_yet_to_submit = KC_NO;

    // Key event queue used as an inter-thread buffer between the client thread
    // (main_thread) and usb_thread.
    key_event_queue_t m_key_event_queue { in_lock };

    // Try to report a key event within the current packet frame, return false if not
    // possible.
    bool try_report_event(uint8_t keycode, bool is_press);

    // Report a key event during the current packet frame if possible. Otherwise put it
    // in the key event queue so that it can be reported in next packet frame(s).
    void report_event(uint8_t keycode, bool is_press);

    // Timer to wait for USB_SUSPEND_EVENT_TIMEOUT_MS before clearing the key event
    // queue, used when events are backfilled while USB is inaccessible.
    // Note: The key event queue is cleared by posting m_event_clear_queue from the
    // timer callback. Directly clearing it in the timer callback (from interrupt
    // context) would require irq_disable()/irq_enable() wrapping for thread safety.
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

    // Submit the current packet frame (report) to the host.
    void submit_report();

    // Called after a packet frame is delivered to the host — whether delivery succeeded,
    // failed, or USB has just become accessible. It will choose the appropriate
    // follow-up action.
    void on_transfer_complete(bool was_successful) override;

    // Used by submit_report() to copy the current report to in_buf.
    virtual void fill_in_buf() =0;

    // Clear the report forcefully.
    virtual void clear_report() =0;

    // Update the current packet frame (report) and return true if successful.
    // Internally used by report_press/release().
    virtual bool update_report(uint8_t keycode, bool is_press) =0;

    bool help_update_bits(uint8_t& bits, uint8_t keycode, bool is_press);
    bool help_update_skro_report(uint8_t keys[], uint8_t keycode, bool is_press);
    bool help_update_nkro_report(uint8_t bits[], uint8_t keycode, bool is_press);

    // Handle a data packet received from the host within the usb_thread context.
    // Typically used to process SET_REPORT requests.
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

    void clear_report() override {
        __builtin_memset(m_report.raw, 0, KEYBOARD_EPSIZE);
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
    // Defines the endpoint report size advertised in the HID report descriptor.
    // While NKRO_REPORT_SIZE reflects full NKRO capability, the actual report format
    // depends on the protocol:
    // - Report protocol: full NKRO report is used.
    // - Boot protocol: falls back to standard 6KRO format.
    // See how xkro_report_key() changes its behavior.
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

    void clear_report() override {
        __builtin_memset(m_report.raw, 0, KEYBOARD_EPSIZE);
    }

    bool update_report(uint8_t keycode, bool is_press) override {
        return keycode >= KC_LCTRL
            ? help_update_bits(m_report.mods, keycode, is_press)
            : (this->*xkro_report_key)(keycode, is_press);
    }

    // Pointer to the active report method, selected based on the current protocol.
    bool (usbus_hid_keyboard_tl::*xkro_report_key)(uint8_t keycode, bool is_press) =
        &usbus_hid_keyboard_tl::nkro_report_key;

    bool skro_report_key(uint8_t keycode, bool is_press) {
        return help_update_skro_report(m_report.keys, keycode, is_press);
    }

    bool nkro_report_key(uint8_t keycode, bool is_press) {
        return help_update_nkro_report(m_report.bits, keycode, is_press);
    }
};
