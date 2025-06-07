#pragma once

#include "event.h"              // for event_post()
#include "thread.h"             // for thread_t
#include "thread_flags.h"       // for thread_flags_set()



class usbhub_thread {
public:
    static void init();

    static void signal_usb_suspend() { thread_flags_set(m_pthread, FLAG_USB_SUSPEND); }

    static void signal_usb_resume() { thread_flags_set(m_pthread, FLAG_USB_RESUME); }

    static void request_usbport_switchover() {
        thread_flags_set(m_pthread, FLAG_USBPORT_SWITCHOVER);
    }

    static void request_extra_enable_manually() {
        thread_flags_set(m_pthread, FLAG_EXTRA_MANUAL);
    }

    static void request_extra_enable_automatically() {
        thread_flags_set(m_pthread, FLAG_EXTRA_AUTOMATIC);
    }

    // Notify that the v_5v report is ready.
    static void signal_v_5v_report() { thread_flags_set(m_pthread, FLAG_REPORT_V_5V); }

    // Notify that the v_con report is ready.
    static void signal_v_con_report() { thread_flags_set(m_pthread, FLAG_REPORT_V_CON); }

    // Signal a generic event to usbhub_thread.
    static void signal_event(event_t* event) { event_post(&m_queue, event); }

private:
    constexpr usbhub_thread() =delete;  // Ensure a static class

    enum : thread_flags_t {
        FLAG_EVENT              = 0x0001,  // == THREAD_FLAG_EVENT from event.h
        FLAG_USB_SUSPEND        = 0x0002,
        FLAG_USB_RESUME         = 0x0004,
        FLAG_USBPORT_SWITCHOVER = 0x0008,
        FLAG_REPORT_V_5V        = 0x0010,
        FLAG_REPORT_V_CON       = 0x0020,
        FLAG_EXTRA_MANUAL       = 0x0040,
        FLAG_EXTRA_AUTOMATIC    = 0x0080,
        FLAG_TIMEOUT            = THREAD_FLAG_TIMEOUT  // (1u << 14)
    };

    static thread_t* m_pthread;

    static char m_thread_stack[];

    static event_queue_t m_queue;

    static void* _thread_entry(void* arg);
};
