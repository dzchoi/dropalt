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
    // It would be rare that more than two (= DEFERRED_PRESSES_SIZE) key presses are made
    // without releasing them yet, while a preceding key is held down.
    m_deferred_presses.reserve(DEFERRED_PRESSES_SIZE);

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
                that->help_handle_key_press(ppmap);
                break;

            case EVENT_KEY_RELEASE:
                that->help_handle_key_release(ppmap);
                break;

            case EVENT_TIMEOUT:
                that->help_handle_timeout(ppmap);
                break;

            default:
                DEBUG("Keymap:\e[1;31m unknown message type (%u)\e[0m\n", msg.type);
                assert( false );
                break;
        }
    }

    return nullptr;
}

void keymap_thread::help_handle_key_press(key::pmap_t* ppmap)
{
    if ( m_behavior_defer_presses > 0 ) {
        m_deferred_presses.push_back(ppmap);
        DEBUG("Keymap: defer press (%p)\n", ppmap);
    }
    else {
        // Todo: ANY can be more than an observer. Its on_press(ppmap) can call
        //  (*ppmap)->on_press(ppmap) inside.
        key::ANY.on_press(ppmap);
        (*ppmap)->on_press(ppmap);
    }
}

void keymap_thread::help_handle_key_release(key::pmap_t* ppmap)
{
    if ( m_behavior_defer_presses > 0 && is_press_deferred(ppmap) )
        flush_deferred_presses(ppmap);

    (*ppmap)->on_release(ppmap);
    key::ANY.on_release(ppmap);

    // In the case m_behavior_defer_presses is disabled from flush_deferred_presses()
    // (i.e. on_other_press()) or on_release() above, we flush all (remaining) deferred
    // presses now.
    if ( m_behavior_defer_presses == 0 && !m_deferred_presses.empty() )
        flush_deferred_presses();
}

void keymap_thread::help_handle_timeout(key::pmap_t* ppmap)
{
    key::timer_t* const ptimer = (*ppmap)->get_timer();
    assert( ptimer != nullptr );

    if ( !ptimer->timeout_expected() ) {
        DEBUG("Keymap:\e[0;34m spurious timeout (ppmap=%p)\e[0m\n", ppmap);
        return;
    }

    // Timeout event is not deferred and handled immediately.
    ptimer->on_timeout(ppmap);

    // In the case m_behavior_defer_presses is disabled from on_timeout() above, we flush
    // all deferred presses now.
    if ( m_behavior_defer_presses == 0 && !m_deferred_presses.empty() )
        flush_deferred_presses();
}

bool keymap_thread::is_press_deferred(key::pmap_t* ppmap) const
{
    for ( key::pmap_t* _ppmap: m_deferred_presses )
        if ( _ppmap == ppmap )
            return true;
    return false;
}

void keymap_thread::flush_deferred_presses()
{
    for ( key::pmap_t* ppmap: m_deferred_presses ) {
        key::ANY.on_press(ppmap);
        DEBUG("Keymap: complete the deferred press (%p)\n", ppmap);
        (*ppmap)->on_press(ppmap);
    }

    m_deferred_presses.clear();
}

void keymap_thread::flush_deferred_presses(key::pmap_t* ppmap)
{
    auto it = m_deferred_presses.begin();
    while ( it != m_deferred_presses.end() ) {
        key::pmap_t* const _ppmap = *it;
        it = m_deferred_presses.erase(it);

        key::ANY.on_press(_ppmap);
        DEBUG("Keymap: complete the deferred press (%p)\n", _ppmap);
        (*_ppmap)->on_press(_ppmap);
        // Note that `it` is never invalidated as on_*_press/release() cannot call
        // flush_deferred_presses() directly or indirectly.

        if ( _ppmap == ppmap )
            break;
    }
}
