#pragma once

#include "thread.h"
#include "ztimer.h"             // for ztimer_t



class main_thread {
public:
    static main_thread& obj() {
        static main_thread obj(thread_get_active());
        return obj;
    }

    void init();
    void main();

    void signal_usb_reset();
    void signal_usb_suspend();
    void signal_usb_resume();

    main_thread(const main_thread&) =delete;
    main_thread& operator=(const main_thread&) =delete;

private:
    main_thread(thread_t* pthread): m_pthread(pthread) {}

    thread_t* m_pthread;

    bool m_show_previous_logs = false;

    ztimer_t m_keep_alive = {
        .base = {},
        .callback = &_tmo_keep_alive,
        .arg = this
    };
    static void _tmo_keep_alive(void* arg);
    static constexpr uint32_t KEEP_ALIVE_PERIOD_MS = 1000;

    enum {
        FLAG_EVENT          = 0x0001,  // == THREAD_FLAG_EVENT from event.h
        FLAG_USB_RESET      = 0x0002,
        FLAG_USB_SUSPEND    = 0x0004,
        FLAG_USB_RESUME     = 0x0008,
        FLAG_TIMEOUT        = THREAD_FLAG_TIMEOUT  // (1u << 14)
    };

    void execute_emulator(void);
};
