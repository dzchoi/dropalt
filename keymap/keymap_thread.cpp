#include "board.h"              // for THREAD_PRIO_KEYMAP

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "keymap.hpp"
#include "keymap_thread.hpp"
#include "observer.hpp"         // for key::ANY
#include "timer.hpp"



void keymap_thread::signal_key_event(key::pbase_t* ppbase, bool pressed)
{
    msg_t msg;
    msg.type = pressed ? EVENT_KEY_PRESS : EVENT_KEY_RELEASE;
    msg.content.ptr = ppbase;
    // will block the caller (matrix_thread) until m_queue has a room if it is full.
    const int ok = msg_send(&msg, m_pid);
    if ( ok != 1 )
        DEBUG("Keymap:\e[1;31m msg_send() returns %d (queued msgs=%u)\e[0m\n",
            ok, msg_avail_thread(m_pid));
}

void keymap_thread::signal_timeout(key::pbase_t* ppbase)
{
    msg_t msg;
    msg.type = EVENT_TIMEOUT;
    msg.content.ptr = ppbase;
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
        key::pbase_t* const ppbase = static_cast<key::pbase_t*>(msg.content.ptr);

        switch ( msg.type ) {
            case EVENT_KEY_PRESS:
                that->help_handle_key_press(ppbase);
                break;

            case EVENT_KEY_RELEASE:
                that->help_handle_key_release(ppbase);
                break;

            case EVENT_TIMEOUT:
                that->help_handle_timeout(ppbase);
                break;

            default:
                DEBUG("Keymap:\e[1;31m unknown message type (%u)\e[0m\n", msg.type);
                assert( false );
                break;
        }
    }

    return nullptr;
}

void keymap_thread::help_handle_key_press(key::pbase_t* ppbase)
{
    if ( m_behavior_defer_presses > 0 ) {
        m_deferred_presses.push_back(ppbase);
        DEBUG("Keymap: defer press (%p)\n", ppbase);
    }
    else {
        // Todo: ANY can be more than an observer. Its on_press(ppbase) can call
        //  (*ppbase)->on_press(ppbase) inside.
        key::ANY.on_press(ppbase);
        (*ppbase)->on_press(ppbase);
    }
}

void keymap_thread::help_handle_key_release(key::pbase_t* ppbase)
{
    if ( m_behavior_defer_presses > 0 && is_press_deferred(ppbase) )
        flush_deferred_presses(ppbase);

    (*ppbase)->on_release(ppbase);
    key::ANY.on_release(ppbase);

    // In the case m_behavior_defer_presses is disabled from flush_deferred_presses()
    // (i.e. on_other_press()) or on_release() above, we flush all (remaining) deferred
    // presses now.
    if ( m_behavior_defer_presses == 0 && !m_deferred_presses.empty() )
        flush_deferred_presses();
}

void keymap_thread::help_handle_timeout(key::pbase_t* ppbase)
{
    key::timer_t* const ptimer = (*ppbase)->get_timer();
    assert( ptimer != nullptr );

    if ( !ptimer->timeout_expected() ) {
        DEBUG("Keymap:\e[0;34m spurious timeout (%p)\e[0m\n", ppbase);
        return;
    }

    // Timeout event is not deferred and handled immediately.
    ptimer->on_timeout(ppbase);

    // In the case m_behavior_defer_presses is disabled from on_timeout() above, we flush
    // all deferred presses now.
    if ( m_behavior_defer_presses == 0 && !m_deferred_presses.empty() )
        flush_deferred_presses();
}

bool keymap_thread::is_press_deferred(key::pbase_t* ppbase) const
{
    for ( key::pbase_t* _ppbase: m_deferred_presses )
        if ( _ppbase == ppbase )
            return true;
    return false;
}

void keymap_thread::flush_deferred_presses()
{
    for ( key::pbase_t* ppbase: m_deferred_presses ) {
        key::ANY.on_press(ppbase);
        DEBUG("Keymap: complete the deferred press (%p)\n", ppbase);
        (*ppbase)->on_press(ppbase);
    }

    m_deferred_presses.clear();
}

void keymap_thread::flush_deferred_presses(key::pbase_t* ppbase)
{
    auto it = m_deferred_presses.begin();
    while ( it != m_deferred_presses.end() ) {
        key::pbase_t* const _ppbase = *it;
        it = m_deferred_presses.erase(it);

        key::ANY.on_press(_ppbase);
        DEBUG("Keymap: complete the deferred press (%p)\n", _ppbase);
        (*_ppbase)->on_press(_ppbase);
        // Note that `it` is never invalidated as on_*_press/release() cannot call
        // flush_deferred_presses() directly or indirectly.

        if ( _ppbase == ppbase )
            break;
    }
}
