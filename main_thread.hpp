#pragma once

#include "thread.h"             // for thread_t
#include "thread_flags.h"       // for thread_flags_t



class main_thread {
public:
    static void init();

    static void signal_usb_reset() { set_thread_flags(FLAG_USB_RESET); }

    static void signal_usb_suspend() { set_thread_flags(FLAG_USB_SUSPEND); }

    static void signal_usb_resume() { set_thread_flags(FLAG_USB_RESUME); }

    static void signal_dte_state(bool dte_enabled);

    static void signal_dte_ready(uint8_t log_mask);

private:
    constexpr main_thread() =delete;  // Ensure a static class

    enum : thread_flags_t {
        FLAG_EVENT          = 0x0001,  // == THREAD_FLAG_EVENT from event.h
        FLAG_USB_RESET      = 0x0002,
        FLAG_USB_SUSPEND    = 0x0004,
        FLAG_USB_RESUME     = 0x0008,
        FLAG_DTE_DISABLED   = 0x0010,
        FLAG_DTE_ENABLED    = 0x0020,
        FLAG_DTE_READY      = 0x0040,
        FLAG_TIMEOUT        = THREAD_FLAG_TIMEOUT  // (1u << 14)
    };

    static void set_thread_flags(thread_flags_t flags);

    static thread_t* m_pthread;

    static void* _thread_entry(void* arg);

    static constexpr uint32_t HEARTBEAT_PERIOD_MS = 1000;
};
