#include "board.h"              // for THREAD_PRIO_KEYMAP

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "adc_thread.hpp"       // for signal_usbport_switchover()
#include "keymap_thread.hpp"
#include "manager.hpp"          // for key::manager
#include "timer.hpp"            // for timer_t::handle_timeout()



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

void keymap_thread::signal_timeout(key::timer_t* ptimer)
{
    msg_t msg;
    msg.type = EVENT_TIMEOUT;
    msg.content.ptr = ptimer;
    // will miss to send if m_queue is full (as being called from interrupt context.)
    const int ok = msg_send(&msg, m_pid);
    assert( ok == 1 ); (void)ok;
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
        assert( ok == 1 ); (void)ok;
    }
}

void keymap_thread::stop_defer_presses()
{
    if ( m_behavior_defer_presses > 0 && --m_behavior_defer_presses == 0 ) {
        msg_t msg;
        msg.type = EVENT_STOP_DEFER_PRESSES;
        const int ok = msg_send(&msg, m_pid);
        assert( ok == 1 ); (void)ok;
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

        switch ( msg.type ) {
            case EVENT_KEY_PRESS: {
                key::pmap_t* const slot = static_cast<key::pmap_t*>(msg.content.ptr);
                manager.handle_press(slot);
                break;
            }

            case EVENT_KEY_RELEASE: {
                key::pmap_t* const slot = static_cast<key::pmap_t*>(msg.content.ptr);
                manager.handle_release(slot);

                if ( that->m_switchover_requested && !manager.is_any_pressing() ) {
                    adc_thread::obj().signal_usbport_switchover();
                    that->m_switchover_requested = false;
                }
                break;
            }

            case EVENT_TIMEOUT:
                key::timer_t::handle_timeout(msg.content.ptr);
                break;

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
