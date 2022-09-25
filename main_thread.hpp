#pragma once

#include "thread.h"
#include "thread_flags.h"

#include "features.hpp"



class main_thread {
public:
    static main_thread& obj() {
        static main_thread obj(thread_get_active());
        return obj;
    }

    void signal_usb_reset() { thread_flags_set(m_pthread, FLAG_USB_RESET); };
    void signal_usb_suspend() { thread_flags_set(m_pthread, FLAG_USB_SUSPEND); };
    void signal_usb_resume() { thread_flags_set(m_pthread, FLAG_USB_RESUME); };

    main_thread(const main_thread&) =delete;
    main_thread& operator=(const main_thread&) =delete;

private:
    main_thread(thread_t* pthread): m_pthread(pthread) {}

    thread_t* m_pthread;

    enum {
        FLAG_EVENT          = 0x0001,  // == THREAD_FLAG_EVENT from event.h
        FLAG_USB_RESET      = 0x0002,
        FLAG_USB_SUSPEND    = 0x0004,
        FLAG_USB_RESUME     = 0x0008,
        FLAG_TIMEOUT        = THREAD_FLAG_TIMEOUT  // (1u << 14)
    };
};
