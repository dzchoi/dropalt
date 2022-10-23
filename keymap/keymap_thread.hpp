#pragma once

#include "msg.h"
#include "thread.h"



namespace key {
class pmap_t;
}

// The keymap_thread receives (physical) key events such as presses/releases from matrix_
// thread, maps them, and finally reports them to usbus_hid_keyboard (i.e. usb_thread).

class keymap_thread {
public:
    static keymap_thread& obj() {
        static keymap_thread obj;
        return obj;
    }

    // Handle input:
    void signal_key_event(key::pmap_t* ppmap, bool pressed);
    void signal_timeout(key::pmap_t* ppmap);

    // Handle output:
    // Output is handled by key::map_t::send_press/tap/release(). See keymap.hpp.

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

    // message types
    enum : uint16_t {
        EVENT_KEY_PRESS = 1,
        EVENT_KEY_TAP,
        EVENT_KEY_RELEASE,
        EVENT_TIMEOUT,
    };
};
