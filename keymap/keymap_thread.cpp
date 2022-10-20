#include "board.h"              // for THREAD_PRIO_KEYMAP

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "keymap.hpp"
#include "keymap_thread.hpp"
#include "observer.hpp"         // for key::ANY
#include "timer.hpp"



void keymap_thread::signal_key_event(key::pmap_t* ppmap, bool pressed)
{
    msg_t msg;
    msg.type = pressed ? EVENT_KEY_PRESS : EVENT_KEY_RELEASE;
    msg.content.ptr = ppmap;
    // will block the caller (matrix_thread) until m_queue has a room if it is full.
    const int ok = msg_send(&msg, m_pid);
    if ( ok != 1 )
        DEBUG("Keymap:\e[1;31m msg_send() returns %d (queued msgs=%u)\e[0m\n",
            ok, msg_avail_thread(m_pid));
}

void keymap_thread::signal_timeout(key::pmap_t* ppmap)
{
    msg_t msg;
    msg.type = EVENT_TIMEOUT;
    msg.content.ptr = ppmap;
    // will miss to send if m_queue is full (as being called from interrupt context.)
    const int ok = msg_send(&msg, m_pid);
    if ( ok != 1 )
        DEBUG("Keymap:\e[1;31m msg_send_int() returns %d (queued msgs=%u)\e[0m\n",
            ok, msg_avail_thread(m_pid));
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

void* keymap_thread::_keymap_thread(void* arg)
{
    keymap_thread* const that = static_cast<keymap_thread*>(arg);

    // The msg_init_queue() should be called from the queue-owning thread.
    msg_init_queue(that->m_queue, KEY_EVENT_QUEUE_SIZE);

    msg_t msg;
    while ( true ) {
        msg_receive(&msg);
        key::pmap_t* const ppmap = static_cast<key::pmap_t*>(msg.content.ptr);

        switch ( msg.type ) {
            case EVENT_KEY_PRESS:
                key::ANY.on_press(ppmap);
                (*ppmap)->on_press(ppmap);
                break;

            case EVENT_KEY_RELEASE:
                (*ppmap)->on_release(ppmap);
                key::ANY.on_release(ppmap);
                break;

            // Todo: Implement it.
            // case EVENT_KEY_TAP:

            case EVENT_TIMEOUT: {
                key::timer_t* const ptimer = (*ppmap)->get_timer();
                assert( ptimer != nullptr );

                if ( ptimer->timeout_expected() )
                    ptimer->on_timeout(ppmap);
                else
                    DEBUG("Keymap:\e[0;34m spurious timeout (ppmap=%p)\e[0m\n", ppmap);
                break;
            }

            default:
                DEBUG("Keymap:\e[1;31m unknown message type (%u)\e[0m\n", msg.type);
                assert( false );
                break;
        }
    }

    return nullptr;
}
