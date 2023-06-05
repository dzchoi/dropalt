#include "board.h"              // for THREAD_PRIO_KEYMAP
#include "event.h"              // for event_queue_init(), event_get(), ...
#include "log.h"
#include "msg.h"                // for msg_send(), msg_try_receive(), 
#include "thread.h"             // for thread_create(), thread_get_unchecked(), ...
#include "thread_flags.h"       // for thread_flags_set(), thread_flags_wait_any(),

#include "adc_thread.hpp"       // for signal_usbport_switchover()
#include "keymap_thread.hpp"
#include "manager.hpp"          // for key::manager



using key::manager;

void keymap_thread::signal_key_event(key::pmap_t* slot, bool pressed)
{
    msg_t msg;
    msg.type = pressed;
    msg.content.ptr = slot;
    // will block the caller (matrix_thread) until m_msg_queue has a room if it is full.
    msg_send(&msg, m_pid);  // will always return 1.
}

keymap_thread::keymap_thread()
{
    m_pid = thread_create(
        m_stack, sizeof(m_stack),
        THREAD_PRIO_KEYMAP,
        THREAD_CREATE_STACKTEST,
        _keymap_thread, this, "keymap_thread");

    m_pthread = thread_get_unchecked(m_pid);
}

void* keymap_thread::_keymap_thread(void* arg)
{
    keymap_thread* const that = static_cast<keymap_thread*>(arg);

    // The event_queue_init() should be called from the queue-owning thread.
    event_queue_init(&that->m_event_queue);

    // The msg_init_queue() should be called from the queue-owning thread.
    msg_init_queue(that->m_msg_queue, KEY_EVENT_QUEUE_SIZE);

    msg_t msg;
    while ( true ) {
        // Zzz
        thread_flags_t flags = thread_flags_wait_any(
            FLAG_EVENT
            | FLAG_MSG_WAITING );

        // Timeout event is handled through Event queue, rather than using m_msg_queue
        // which is used for buffering key inputs. See the note below.
        if ( flags & FLAG_EVENT ) {
            event_t* event;
            while ( (event = event_get(&that->m_event_queue)) != nullptr )
                event->handler(event);
        }

        if ( flags & FLAG_MSG_WAITING ) {
            if ( msg_try_receive(&msg) == 1 ) {
                key::pmap_t* const slot = static_cast<key::pmap_t*>(msg.content.ptr);

                if ( msg.type )  // msg.type == 1 for press
                    manager.handle_press(slot);

                else {  // msg.type == 0 for release
                    manager.handle_release(slot);

                    // Execute switchover.
                    if ( that->m_switchover_requested && !manager.is_any_pressing() ) {
                        adc_thread::obj().signal_usbport_switchover();
                        that->m_switchover_requested = false;
                    }
                }
            }

            // Key events are processed one at a time in order to process internal events
            // with higher priority.
            if ( msg_avail() > 0 )
                thread_flags_set(that->m_pthread, FLAG_MSG_WAITING);
        }

        // Complete any deferred key presses if we are no longer deferring. Note that in
        // the meanwhile presses can be deferred again.
        manager.complete_if_not_deferring();

        if ( !manager.is_any_pressing() )
            LOG_DEBUG("Keymap: ---- all handled\n");
    }

    return nullptr;
}

// Note about Msg queue vs Event queue.
//  - Msg queue can be full but event queue cannot.
//  - When Msg queue is full msg_send() blocks. However, if msg_send() is called from
//    the context of the thread that owns the Msg queue (in this case msg_send_to_self()
//    is called), or if msg_send() is called from an interrupt context (msg_send_int() is
//    called), it does not block but returns an error (0). In those cases msg_send() is
//    the same as msg_try_send().
//  - So, Msg queue is mostly for interfacing to other threads and can make them wait
//    until the queue is available.
//  - Avoid mixing external events (where queue full is acceptable) with internal events
//    (queue full is not acceptable).
//  - Event is just like a thread signal but can be accompanied by argument(s). There
//    is no reason to use event with no argument over a thread signal. However, beware of
//    using the same Event struct with different arguments. Previous event will be
//    overwritten when a new event only with different argument is pushed.
