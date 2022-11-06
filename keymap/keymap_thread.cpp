#include "board.h"              // for THREAD_PRIO_KEYMAP

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "adc_thread.hpp"       // for signal_usbhub_switchover()
#include "keymap.hpp"
#include "keymap_thread.hpp"
#include "observer.hpp"         // for key::ANY
#include "pressing_list.hpp"
#include "timer.hpp"
#include "usb_descriptor.hpp"   // for SKRO_KEYS_SIZE



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
    assert( ok == 1 );
}

keymap_thread::keymap_thread()
{
    // Mostly we would press not more than 6 keys simultaneously, but the pressing list
    // will be expanded as needed.
    key::pressing_list::reserve(SKRO_KEYS_SIZE);

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

static constexpr auto execute_press = [](key::pmap_t* ppmap) {
    key::ANY.on_press(ppmap);
    DEBUG("Keymap: complete the deferred press (%p)\n", ppmap);
    (*ppmap)->on_press(ppmap);
};

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

            case EVENT_START_DEFER_PRESSES:
                key::pressing_list::start_defering();
                break;

            case EVENT_STOP_DEFER_PRESSES:
                key::pressing_list::complete_deferred(execute_press);
                key::pressing_list::stop_defering();
                break;

            default:
                assert( false );
        }
    }

    return nullptr;
}

void keymap_thread::help_handle_key_press(key::pmap_t* ppmap)
{
    key::pressing_list::add(ppmap);

    if ( key::pressing_list::is_deferring() ) {
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
    if ( key::pressing_list::is_deferring(ppmap) )
        key::pressing_list::complete_deferred(execute_press, ppmap);

    (*ppmap)->on_release(ppmap);
    key::ANY.on_release(ppmap);

    key::pressing_list::remove(ppmap);

    if ( m_switchover_requested && !key::pressing_list::is_any_pressing() ) {
        adc_thread::obj().signal_usbhub_switchover();
        m_switchover_requested = false;
    }
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
}
