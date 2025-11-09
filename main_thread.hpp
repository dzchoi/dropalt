#pragma once

#include "thread.h"             // for thread_t, thread_get_active()
#include "thread_flags.h"       // for thread_flags_t
#include "ztimer.h"             // for ztimer_t

#include "event_ext.hpp"        // for event_queue_t

// Physical key press/release events originate in matrix_thread, are forwarded to
// main_thread for conversion into USB keycodes, and finally delivered to usb_thread
// via usb_thread::send_press/release().



class main_thread {
public:
    static void init();

    // Check if we are in the main_thread context.
    static bool is_active() { return thread_get_active() == m_pthread; }

    static bool is_dfu_mode() { return m_active_mode == &dfu_mode; }

    // Signal to main_thread that another thread (matrix_ or usb_thread) is now idle.
    static void signal_thread_idle();

    static void signal_usb_reset() { set_thread_flags(FLAG_USB_RESET); }

    static void signal_usb_suspend() { set_thread_flags(FLAG_USB_SUSPEND); }

    static void signal_usb_resume() { set_thread_flags(FLAG_USB_RESUME); }

    static void signal_dte_state(bool dte_enabled);

    static void signal_dte_ready(uint8_t log_mask);

    static void signal_mode_toggle() { set_thread_flags(FLAG_MODE_TOGGLE); }

    // Signal a generic event to main_thread.
    static void signal_event(event_t* event);

    // For non-zero timeout_us it returns an indicator of whether or not it has been
    // signaled successfully. If it is zero it waits indefinitely and returns true.
    static bool signal_key_event(unsigned slot_index, bool is_press, uint32_t timeout_us =0);

    static void signal_lamp_state(uint8_t lamp_state);

private:
    constexpr main_thread() =delete;  // Ensure a static class

    enum : thread_flags_t {
        FLAG_GENERIC_EVENT  = 0x0001,  // == THREAD_FLAG_EVENT from event.h
        FLAG_USB_RESET      = 0x0002,
        FLAG_USB_SUSPEND    = 0x0004,
        FLAG_USB_RESUME     = 0x0008,
        FLAG_DTE_ENABLED    = 0x0010,
        FLAG_DTE_DISABLED   = 0x0020,
        FLAG_DTE_READY      = 0x0040,
        FLAG_KEY_EVENT      = 0x0080,
        FLAG_MODE_TOGGLE    = 0x0100,
        FLAG_TIMEOUT        = THREAD_FLAG_TIMEOUT,  // (1u << 14)

        ALL_FLAGS           = ((FLAG_MODE_TOGGLE << 1) - 1) | FLAG_TIMEOUT
    };

    static void set_thread_flags(thread_flags_t flags);

    static void set_my_flags(thread_flags_t flags);

    static thread_t* m_pthread;

    static char m_thread_stack[];

    // event_t queue for internal events
    static event_queue_t m_event_queue;

    static void* _thread_entry(void* arg);

    static void* dfu_mode(void* arg);
    static void* normal_mode(void* arg);

    // m_active_mode points to the current mode handler, either dfu_mode() or
    // normal_mode().
    static void* (*m_active_mode)(void*);

    // Watchdog refresh timer
    static constexpr uint32_t HEARTBEAT_PERIOD_MS = 1000;
    static ztimer_t m_heartbeat_timer;
};

constexpr const char* press_or_release(bool is_press)
{
    return is_press ? "press" : "release";
}
