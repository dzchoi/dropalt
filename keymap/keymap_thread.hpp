#pragma once

#include "event.h"              // for event_t, event_queue_t, event_post(), ...
#include "msg.h"                // for msg_t
#include "thread.h"             // for thread_t
#include "thread_flags.h"       // for THREAD_FLAG_*

#include <atomic>



namespace key {
class pmap_t;
}

// The keymap_thread receives (physical) key events (presses/releases) from matrix_
// thread, maps them using keymaps, and finally reports them to usbus_hid_keyboard (i.e.
// usb_thread).

class keymap_thread {
public:
    static keymap_thread& obj() {
        static keymap_thread obj;
        return obj;
    }

    void signal_key_event(key::pmap_t* slot, bool pressed);

    // Signal a (generic) event to keymap_thread.
    void signal_event(event_t* event) { event_post(&m_event_queue, event); }

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
    void signal_usbport_switchover() { m_switchover_requested = true; }

private:
    kernel_pid_t m_pid;
    thread_t* m_pthread;

#ifdef DEVELHELP
    char m_stack[THREAD_STACKSIZE_SMALL + THREAD_EXTRA_STACKSIZE_PRINTF];
#else
    char m_stack[THREAD_STACKSIZE_SMALL];
#endif

    // event queue for processing internal events
    event_queue_t m_event_queue;  // event queue

    // message queue for buffering external key input events
    static constexpr size_t KEY_EVENT_QUEUE_SIZE = 16;  // must be a power of two.
    msg_t m_msg_queue[KEY_EVENT_QUEUE_SIZE];

    keymap_thread();  // Can be called only by obj().

    // thread body
    static void* _keymap_thread(void* arg);

    unsigned m_behavior_defer_presses = 0;

    std::atomic<bool> m_switchover_requested = false;

    enum : uint16_t {
        FLAG_EVENT          = THREAD_FLAG_EVENT,       // 0x1
        FLAG_START_DEFER    = 0x2,
        FLAG_STOP_DEFER     = 0x4,
        FLAG_MSG_WAITING    = THREAD_FLAG_MSG_WAITING  // (1u << 15)
    };
};
