#pragma once

#include "msg.h"                // for msg_t
#include "thread_flags.h"       // for thread_flags_set()

#include "config.hpp"           // for RGB_LED_GCR_MAX



// Control the Global Current Control Register (GCR).
class rgb_gcr {
public:
    // Once set, GCR will gradually ramp toward the desired value over time.
    void set(uint8_t desired_gcr) { m_desired_gcr = desired_gcr; }
    void enable() { m_enabled = true; }
    void disable();
    bool is_enabled() const { return m_enabled; }

    // Invoking signal_v_5v_report() from adc::v_5v drives gradual updates to the GCR
    // without having to run a separate timer.
    // Note that SSD is automatically controlled depending on whether GCR is zero or not.
    void process_v_5v_report();

private:
    // All class methods execute in the rgb_thread context, ensuring thread-safe access
    // to these data members.
    bool m_enabled = false;
    uint8_t m_current_gcr = 0;
    uint8_t m_desired_gcr = RGB_LED_GCR_MAX;
};



class rgb_thread {
public:
    static void init();

    static void signal_usb_suspend() { thread_flags_set(m_pthread, FLAG_USB_SUSPEND); }

    static void signal_usb_resume() { thread_flags_set(m_pthread, FLAG_USB_RESUME); }

    static void signal_v_5v_report() {
        if ( m_gcr.is_enabled() )  // Avoid unnecessary signaling.
            thread_flags_set(m_pthread, FLAG_REPORT_V_5V);
    }

    static void signal_key_event(unsigned slot_index, bool is_press) {
        send_msg_event(slot_index, msg_event_t(is_press));
    }

    static void signal_lamp_state(unsigned slot_index) {
        send_msg_event(slot_index, LAMP_CHANGED);
    }

private:
    constexpr rgb_thread() =delete;  // Ensure a static class

    enum : thread_flags_t {
        FLAG_EVENT              = 0x0001,  // not used
        FLAG_USB_SUSPEND        = 0x0002,
        FLAG_USB_RESUME         = 0x0004,
        FLAG_REPORT_V_5V        = 0x0008,
        FLAG_MSG_EVENT          = THREAD_FLAG_MSG_WAITING  // (1u << 15)
    };

    enum msg_event_t: uint16_t {
        KEY_RELEASED            = 0,
        KEY_PRESSED             = 1,
        LAMP_CHANGED            = 2
    };

    static thread_t* m_pthread;

    static char m_thread_stack[];

    // Message queue for buffering key input events
    static msg_t m_msg_queue[];

    static rgb_gcr m_gcr;

    static void* _thread_entry(void* arg);

    static void send_msg_event(unsigned slot_index, msg_event_t event);
};
