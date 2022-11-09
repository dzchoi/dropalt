#pragma once

#include "msg.h"
#include "thread.h"

#include <atomic>



namespace key {
class pmap_t;
class timer_t;
}

// The keymap_thread receives (physical) key events such as presses/releases from matrix_
// thread, maps them using keymaps, and finally reports them to usbus_hid_keyboard (i.e.
// usb_thread).

class keymap_thread {
public:
    static keymap_thread& obj() {
        static keymap_thread obj;
        return obj;
    }

    // Handle input:
    void signal_key_event(key::pmap_t* slot, bool pressed);
    void signal_timeout(key::timer_t* ptimer);

    // Handle output:
    // Output is handled by key::map_t::send_press/release(). See map.hpp.

    // In the behavior of defer-presses, every press is deferred and triggered later
    // when its release occurs or until stop_defer_presses() is called. To preserve the
    // order of presses, however, the release triggers not only its own press but also
    // all the other presses that have occurred and deferred before its press. For
    // example, suppose A, B, C, D and E are pressed and deferred in that order. When C
    // is released A, B and C are pressed before C is released. D and E are left still
    // deferred.
    void start_defer_presses();
    void stop_defer_presses();

    // Switchover will be performed when all keys are released.
    void signal_usbhub_switchover() { m_switchover_requested = true; }

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

    keymap_thread();  // Can be called only by obj().

    // thread body
    static void* _keymap_thread(void* arg);

    unsigned m_behavior_defer_presses = 0;

    std::atomic<bool> m_switchover_requested = false;

    // message types
    enum : uint16_t {
        EVENT_KEY_PRESS = 1,
        EVENT_KEY_RELEASE,
        EVENT_TIMEOUT,
        EVENT_START_DEFER_PRESSES,
        EVENT_STOP_DEFER_PRESSES,
    };
};
