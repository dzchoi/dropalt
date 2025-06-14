#pragma once

#include "thread.h"             // for thread_t
#include "thread_flags.h"       // for thread_flags_t

#include "event_ext.hpp"        // for event_ext_t, event_queue_t, event_post(), ...



class main_thread {
public:
    static void init();

    static void signal_usb_reset() { set_thread_flags(FLAG_USB_RESET); }

    static void signal_usb_suspend() { set_thread_flags(FLAG_USB_SUSPEND); }

    static void signal_usb_resume() { set_thread_flags(FLAG_USB_RESUME); }

    static void signal_dte_state(bool dte_enabled);

    static void signal_dte_ready(uint8_t log_mask);

    // Signal a generic event to main_thread.
    static void signal_event(event_t* event) { event_post(&m_event_queue, event); }

    // For non-zero timeout_us it returns an indicator of whether or not it has been
    // signaled successfully. If it is zero it waits indefinitely and returns true.
    static bool signal_key_event(unsigned slot_index, bool is_press, uint32_t timeout_us =0);

private:
    constexpr main_thread() =delete;  // Ensure a static class

    enum : thread_flags_t {
        FLAG_GENERIC_EVENT  = 0x0001,  // == THREAD_FLAG_EVENT from event.h
        FLAG_KEY_EVENT      = 0x0002,
        FLAG_USB_RESET      = 0x0004,
        FLAG_USB_SUSPEND    = 0x0008,
        FLAG_USB_RESUME     = 0x0010,
        FLAG_DTE_DISABLED   = 0x0020,
        FLAG_DTE_ENABLED    = 0x0040,
        FLAG_DTE_READY      = 0x0080,
        FLAG_TIMEOUT        = THREAD_FLAG_TIMEOUT  // (1u << 14)
    };

    static void set_thread_flags(thread_flags_t flags);

    static thread_t* m_pthread;

    // event_t queue for internal events
    static event_queue_t m_event_queue;

    static void* _thread_entry(void* arg);

    static constexpr uint32_t HEARTBEAT_PERIOD_MS = 1000;
};

constexpr const char* press_or_release(bool is_press)
{
    return is_press ? "press" : "release";
}
