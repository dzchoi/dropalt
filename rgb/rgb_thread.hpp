#pragma once

#include "msg.h"                // for msg_t
#include "thread_flags.h"       // for thread_flags_set()
#include "ztimer.h"             // for ztimer_t

#include "features.hpp"         // for RGB_LED_ENABLE, RGB_LED_GCR_MAX, ...



namespace key {
class pmap_t;
}

class effect_t;
class lamp_t;



template <bool>
class rgb_thread_tl {
public:
    static rgb_thread_tl& obj() {
        static rgb_thread_tl obj;
        return obj;
    }

    void signal_usb_suspend() {}
    void signal_usb_resume() {}
    void signal_report_v_5v() {}
    void signal_key_event(key::pmap_t*, bool) {}
    void signal_lamp_state(lamp_t*) {}

    // Todo: Change attributes of the current effect.
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
    void signal_report_v_5v();
    void signal_key_event(key::pmap_t* slot, bool is_press) {
        send_msg_event(slot, msg_event_t(is_press));
    }
    // Indicator lamps will work even if no effect is active.
    void signal_lamp_state(lamp_t* plamp) { send_msg_event(plamp, LAMP_CHANGED); }

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
        bool is_enabled() const { return m_enabled; }
        void enable() { m_enabled = true; }
        void disable();

        bool raise() {
            return (m_desired_gcr < RGB_LED_GCR_MAX) && (++m_desired_gcr, true); }
        bool lower() {
            return (m_desired_gcr > 0) && (--m_desired_gcr, true); }

        // SSD gets also locked or released according to whether gcr is zero or not.
        void adjust();

    private:
        bool m_enabled = false;
        uint8_t m_current_gcr = 0;
        uint8_t m_desired_gcr = 0;
    } m_gcr;

    enum : uint16_t {
        FLAG_EVENT              = 0x0001,  // not used
        FLAG_USB_SUSPEND        = 0x0002,
        FLAG_USB_RESUME         = 0x0004,
        FLAG_ADJUST_GCR         = 0x0008,
        FLAG_SET_EFFECT         = 0x0010,
        FLAG_TIMEOUT            = THREAD_FLAG_TIMEOUT,     // (1u << 14)
        FLAG_MSG_EVENT          = THREAD_FLAG_MSG_WAITING  // (1u << 15)
    };

    effect_t* m_peffect = nullptr;

    enum msg_event_t: uint16_t {
        KEY_RELEASED    = 0,
        KEY_PRESSED     = 1,
        LAMP_CHANGED    = 2
    };

    void send_msg_event(void* ptr, msg_event_t event);
    void process_msg_event(void* ptr, msg_event_t event);

    void initialize_effect();
    void refresh_effect();
};

using rgb_thread = rgb_thread_tl<RGB_LED_ENABLE>;
