#pragma once

#include "msg.h"
#include "thread.h"

#include <vector>               // for std::vector<> from Newlib



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
    // Output is handled by key::map_t::send_press/release(). See keymap.hpp.

    // In the behavior of defer-presses, every press is deferred and triggered when its
    // release occurs or until stop_defer_presses() is called. To preserve the order of
    // presses, however, the release triggers not only its own press but also all other
    // presses that have occurred and deferred before its own press. For example,
    // suppose if A, B, C, D and E are pressed and deferred in that order. When C is
    // released A, B and C are pressed before C is released. D and E are left still
    // deferred.
    void start_defer_presses() { m_behavior_defer_presses++; }

    void stop_defer_presses() {
        if ( m_behavior_defer_presses > 0 )
            m_behavior_defer_presses--;
    }

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
    std::vector<key::pmap_t*> m_deferred_presses;
    static constexpr size_t DEFERRED_PRESSES_SIZE = 2;

    // Check if ppmap's press has been deferred.
    bool is_press_deferred(key::pmap_t* ppmap) const;

    // Complete all the deferred presses.
    void flush_deferred_presses();

    // Complete the deferred presses up to ppmap.
    void flush_deferred_presses(key::pmap_t* ppmap);

    // message types
    enum : uint16_t {
        EVENT_KEY_PRESS = 1,
        EVENT_KEY_RELEASE,
        EVENT_TIMEOUT,
    };

    void help_handle_key_press(key::pmap_t* ppmap);
    void help_handle_key_release(key::pmap_t* ppmap);
    void help_handle_timeout(key::pmap_t* ppmap);
};