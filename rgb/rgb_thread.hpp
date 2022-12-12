#pragma once

#include "msg.h"                // for msg_t
#include "thread.h"
#include "thread_flags.h"       // for thread_flags_set()
#include "ztimer.h"             // for ztimer_t

#include "features.hpp"         // for RGB_LED_ENABLE



class effect_t;



template <bool>
class rgb_thread_tl {
public:
    static rgb_thread_tl& obj() {
        static rgb_thread_tl obj;
        return obj;
    }

    void signal_usb_suspend() {}
    void signal_usb_resume() {}
    void signal_key_event(uint8_t, bool) {}

    void set_effect(const effect_t&) {}

    rgb_thread_tl(const rgb_thread_tl&) =delete;
    rgb_thread_tl& operator=(const rgb_thread_tl&) =delete;

private:
    rgb_thread_tl() {}  // Can be called only by obj().
};



template <>
class rgb_thread_tl<true> {
public:
    static rgb_thread_tl& obj() {
        static rgb_thread_tl obj;
        return obj;
    }

    void signal_usb_suspend() { thread_flags_set(m_pthread, FLAG_USB_SUSPEND); }
    void signal_usb_resume() { thread_flags_set(m_pthread, FLAG_USB_RESUME); }
    void signal_key_event(uint8_t led_id, bool pressed);

    // Todo: Effect for underglow leds?
    void set_effect(effect_t&);

    rgb_thread_tl(const rgb_thread_tl&) =delete;
    rgb_thread_tl& operator=(const rgb_thread_tl&) =delete;

private:
    kernel_pid_t m_pid;
    thread_t* m_pthread;

#ifdef DEVELHELP
    char m_stack[THREAD_STACKSIZE_MEDIUM];
#else
    char m_stack[THREAD_STACKSIZE_SMALL];
#endif

    // message queue for buffering input key events
    static constexpr size_t KEY_EVENT_QUEUE_SIZE = 16;  // must be a power of two.
    msg_t m_queue[KEY_EVENT_QUEUE_SIZE];

    rgb_thread_tl();  // Can be called only by obj().

    // thread body
    static void* _rgb_thread(void* arg);

    // Global Current Control Register (GCR) value
    class gcr_t {
    public:
        void set(uint8_t gcr);
        void clear();
        void change();
        // bool is_set() const { return m_current_gcr > 0; }

    private:
        uint8_t m_current_gcr = 0;
        uint8_t m_desired_gcr = 0;

        // Todo: m_gcr_limit to limit the max gcr value, changing as reading v_5v.

        ztimer_t m_timer = {
            .base = {},
            .callback = [](void*){
                thread_flags_set(obj().m_pthread, FLAG_CHANGE_GCR);
            },
            .arg = nullptr };
    } m_gcr;

    enum : uint16_t {
        FLAG_EVENT          = 0x0001,  // not used
        FLAG_USB_SUSPEND    = 0x0002,
        FLAG_USB_RESUME     = 0x0004,
        FLAG_CHANGE_GCR     = 0x0008,
        FLAG_SET_EFFECT     = 0x0010,
        FLAG_TIMEOUT        = THREAD_FLAG_TIMEOUT,     // (1u << 14)
        FLAG_KEY_EVENT      = THREAD_FLAG_MSG_WAITING  // (1u << 15)
    };

    effect_t* m_peffect = nullptr;

    void initialize_effect();
    void process_key_event(uint8_t led_id, bool pressed);
    void refresh_effect();
};

using rgb_thread = rgb_thread_tl<RGB_LED_ENABLE>;
