#include "board.h"              // for THREAD_PRIO_KEYMAP

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "adc_thread.hpp"       // for signal_usbhub_switchover()
#include "map.hpp"              // for map_t::get_timer()
#include "keymap_thread.hpp"
#include "map_timer.hpp"
#include "manager.hpp"          // for key::manager



using key::manager;

void keymap_thread::signal_key_event(key::pmap_t* slot, bool pressed)
{
    msg_t msg;
    msg.type = pressed ? EVENT_KEY_PRESS : EVENT_KEY_RELEASE;
    msg.content.ptr = slot;
    // will block the caller (matrix_thread) until m_queue has a room if it is full.
    const int ok = msg_send(&msg, m_pid);
    if ( ok != 1 )
        DEBUG("Keymap:\e[1;31m msg_send() returns %d (queued msgs=%u)\e[0m\n",
            ok, msg_avail_thread(m_pid));
}

void keymap_thread::signal_timeout(key::pmap_t* slot)
{
    msg_t msg;
    msg.type = EVENT_TIMEOUT;
    msg.content.ptr = slot;
    // will miss to send if m_queue is full (as being called from interrupt context.)
    const int ok = msg_send(&msg, m_pid);
    assert( ok == 1 );
}

keymap_thread::keymap_thread()
{
    m_pid = thread_create(
        m_stack, sizeof(m_stack),
        THREAD_PRIO_KEYMAP,
        THREAD_CREATE_STACKTEST,
        _keymap_thread, this, "keymap_thread");

    m_pthread = thread_get(m_pid);
}

void keymap_thread::start_defer_presses()
{
    if ( m_behavior_defer_presses++ == 0 ) {
        msg_t msg;
        msg.type = EVENT_START_DEFER_PRESSES;
        const int ok = msg_send(&msg, m_pid);
        assert( ok == 1 );
    }
}

void keymap_thread::stop_defer_presses()
{
    if ( m_behavior_defer_presses > 0 && --m_behavior_defer_presses == 0 ) {
        msg_t msg;
        msg.type = EVENT_STOP_DEFER_PRESSES;
        const int ok = msg_send(&msg, m_pid);
        assert( ok == 1 );
    }
}

void* keymap_thread::_keymap_thread(void* arg)
{
    keymap_thread* const that = static_cast<keymap_thread*>(arg);

    // The msg_init_queue() should be called from the queue-owning thread.
    msg_init_queue(that->m_queue, KEY_EVENT_QUEUE_SIZE);

    msg_t msg;
    while ( true ) {
        msg_receive(&msg);
        key::pmap_t* const slot = static_cast<key::pmap_t*>(msg.content.ptr);

        switch ( msg.type ) {
            case EVENT_KEY_PRESS:
                manager.handle_press(slot);
                break;

            case EVENT_KEY_RELEASE:
                manager.handle_release(slot);

                if ( that->m_switchover_requested && !manager.is_any_pressing() ) {
                    adc_thread::obj().signal_usbhub_switchover();
                    that->m_switchover_requested = false;
                }
                break;

            case EVENT_TIMEOUT: {
                key::map_timer_t* const ptimer = (*slot)->get_timer();
                assert( ptimer != nullptr );
                if ( ptimer->timeout_expected() )
                    // Timeout event is not deferred but handled immediately.
                    ptimer->on_timeout(slot);
                else
                    DEBUG("Keymap:\e[0;34m spurious timeout (slot=%p)\e[0m\n", slot);
                break;
            }

            case EVENT_START_DEFER_PRESSES:
                manager.start_defering();
                break;

            case EVENT_STOP_DEFER_PRESSES:
                manager.complete_deferred();
                manager.stop_defering();
                break;

            default:
                assert( false );
        }
    }

    return nullptr;
}
