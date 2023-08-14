#pragma once

#include <stdint.h>             // for uint16_t, uint32_t
#include "thread.h"             // for thread_t, thread_get_active()
#include "thread_flags.h"       // for THREAD_FLAG_EVENT

#include "event_ext.hpp"        // for event_ext_t, event_queue_t, event_post(), ...
#include "key_events.hpp"       // for key::events_t



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

    static key::events_t& key_events() { return obj().m_key_events; }

    // Todo: Will be renamed to map::thread.is_active().
    bool is_active() const { return m_pthread == thread_get_active(); }

    // For non-zero timeout_us it returns an indicator of whether or not it was signaled
    // successfully. If timeout_us is zero it waits indefinitely and returns true.
    bool signal_key_event(size_t index, bool is_press, uint32_t timout_us =0u);

    void signal_lamp_state(key::pmap_t* slot);

    // Signal a (generic) event to keymap_thread.
    void signal_event(event_t* event) { event_post(&m_event_queue, event); }

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

    // Event buffer for external key input events
    key::events_t m_key_events;

    // event_t queue for internal events
    event_queue_t m_event_queue;

    event_ext_t<key::pmap_t*> m_event_lamp_state = {
        nullptr, _hdlr_lamp_state, nullptr };
    static void _hdlr_lamp_state(event_t* event);

    // Constructor
    keymap_thread();  // Can be called only by obj().

    // Thread body
    void* _keymap_thread();

    // Engine for driving all on_*_press/release() functions.
    // Todo: Will be moved to pmap_t::process_key_event().
    void handle_key_event(key::pmap_t* slot, bool is_press);

    bool m_switchover_requested = false;

    enum : uint16_t {
        FLAG_EVENT          = THREAD_FLAG_EVENT,       // 0x1
        FLAG_KEY_EVENT      = 0x0002
    };
};



// Helper function
inline const char* press_or_release(bool is_press)
{
    return is_press ? "press" : "release";
}
