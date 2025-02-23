#pragma once

#include "thread.h"             // for thread_t
#include "thread_flags.h"       // for thread_flags_t



// This "main" class is a singleton. Use main::thread() to access the instance.
class main {
public:
    static void init();

    static main& thread() { return m_instance; }

    enum : thread_flags_t {
        FLAG_EVENT          = 0x0001,  // == THREAD_FLAG_EVENT from event.h
        FLAG_USB_RESET      = 0x0002,
        FLAG_USB_SUSPEND    = 0x0004,
        FLAG_USB_RESUME     = 0x0008,
        FLAG_DTE_DISABLED   = 0x0010,
        FLAG_DTE_ENABLED    = 0x0020,
        FLAG_TIMEOUT        = THREAD_FLAG_TIMEOUT  // (1u << 14)
    };

    void set_thread_flags(thread_flags_t flags);

    void signal_usb_reset() { set_thread_flags(FLAG_USB_RESET); }

    void signal_usb_suspend() { set_thread_flags(FLAG_USB_SUSPEND); }

    void signal_usb_resume() { set_thread_flags(FLAG_USB_RESUME); }

    void signal_dte_state(bool dte_enabled);

private:
    static main m_instance;

    static void* _thread_entry(void* arg);

    static constexpr uint32_t HEARTBEAT_PERIOD_MS = 1000;

    thread_t* m_pthread;

    // bool m_show_previous_logs = false;

    main() =default;  // for creating the static m_instance.
};
