#include "board.h"              // for THREAD_PRIO_KEYMAP, system_reset()
#include "event.h"              // for event_queue_init(), event_get(), ...
#include "irq.h"                // for irq_disable(), irq_restore()
#include "log.h"
#include "thread.h"             // for thread_create(), thread_get_unchecked(), ...
#include "thread_flags.h"       // for thread_flags_set(), thread_flags_wait_any(), ...

#include "adc_thread.hpp"       // for signal_usbport_switchover()
#include "defer.hpp"            // for on_other_press/release()
#include "key_events.hpp"       // for push(), next_event(), ...
#include "keymap_thread.hpp"
#include "lamp.hpp"             // for lamp_t::slot(), execute_lamp()
#include "map.hpp"              // for map_t::press/release()
#include "matrix_thread.hpp"    // for is_any_pressed()
#include "pmap.hpp"             // for key::maps[], on_press/release(), ...
#include "rgb_thread.hpp"       // for signal_lamp_state(), signal_key_event()



using namespace key;

bool keymap_thread::signal_key_event(size_t index, bool is_press, uint32_t timeout_us)
{
    if ( timeout_us )
        LOG_DEBUG("Matrix: %s [%u]\n", press_or_release(is_press), index);
    else
        LOG_INFO("Emulate %s [%u]\n", press_or_release(is_press), index);

    if ( m_key_events.push(index, is_press, timeout_us) ) {
        thread_flags_set(m_pthread, FLAG_KEY_EVENT);
        return true;
    }

    // The failure from m_key_events.push() means that the key event buffer is full with
    // all deferred events. We have no way to recover it but to reboot our keyboard, as
    // keymap_thread cannot take care of this case. (See comment in key_events.hpp).
    // Todo: LOG_ERROR() just before system_reset() seems not working alwways. Might it
    //  need some wait?
    LOG_ERROR("Keymap: m_key_events.push() failed\n");
    system_reset();
    return false;
}

void keymap_thread::signal_lamp_state(lamp_t* plamp)
{
    m_event_lamp_state.arg = plamp;
    signal_event(&m_event_lamp_state);
}

// _hdlr_timeout() will execute in the context of keymap_thread.
void keymap_thread::_hdlr_lamp_state(event_t* event)
{
    lamp_t* const plamp = static_cast<event_ext_t<lamp_t*>*>(event)->arg;
    // Turn on/off the indicator lamp first.
    rgb_thread::obj().signal_lamp_state(plamp);
    // Then execute when_lamp_on/off().
    plamp->execute_lamp();
}

keymap_thread::keymap_thread()
{
    m_pid = thread_create(
        m_stack, sizeof(m_stack),
        THREAD_PRIO_KEYMAP,
        THREAD_CREATE_STACKTEST,
        [](void* arg) { return static_cast<keymap_thread*>(arg)->_keymap_thread(); },
        this, "keymap_thread");

    m_pthread = thread_get_unchecked(m_pid);
}

void* keymap_thread::_keymap_thread()
{
    // The event_queue_init() should be called from the queue-owning thread.
    event_queue_init(&m_event_queue);

    while ( true ) {
        // Zzz
        thread_flags_t flags = thread_flags_wait_any(
            FLAG_EVENT | FLAG_KEY_EVENT);

        // Timeout event and lamp state event are handled through m_event_queue.
        if ( flags & FLAG_EVENT ) {
            while ( event_t* event = event_get(&m_event_queue) )
                event->handler(event);
        }

        // Key events, as having lower priority, are processed one at a time.
        events_t::key_event_t event;
        if ( m_key_events.next_event(&event) )
            handle_key_event(&maps[event.index], event.is_press);

        if ( m_key_events.next_event() ) {
            unsigned state = irq_disable();
            m_pthread->flags |= FLAG_KEY_EVENT;
            irq_restore(state);
            // Or we could use `atomic_fetch_or_u16(&m_pthread->flags, FLAG_KEY_EVENT)`.
        }

        // Execute switchover if everything is idle.
        else if ( m_switchover_requested &&
                  !m_key_events.deferrer() &&
                  !matrix_thread::obj().is_any_pressed() ) {
            adc_thread::obj().signal_usbport_switchover();
            m_switchover_requested = false;
        }
    }

    return nullptr;
}

void keymap_thread::handle_key_event(pmap_t* slot, bool is_press)
{
    const char* const press_or_release = ::press_or_release(is_press);

    // Execute the event if in normal mode.
    if ( !m_key_events.deferrer() ) {
        LOG_DEBUG("Keymap: [%u] handle %s\n", slot->index(), press_or_release);
        rgb_thread::obj().signal_key_event(slot, is_press);
        is_press ? (*slot)->press(slot) : (*slot)->release(slot);
        return;
    }

    // In Defer mode, if the event is the deferrer's own event, execute it now.
    if ( slot == m_key_events.deferrer()->m_slot )
        LOG_DEBUG("Keymap: [%u] handle deferrer %s\n", slot->index(), press_or_release);

    // In Defer mode, if the event is an other key event, notify the deferrer first.
    else {
        LOG_DEBUG("Keymap: [%u] handle other %s [%u]\n",
            m_key_events.deferrer()->m_slot->index(), press_or_release, slot->index());
        if ( is_press
             ? !m_key_events.deferrer()->on_other_press(slot)
             : !m_key_events.deferrer()->on_other_release(slot) )
            return;

        // If the deferrer has decided to no longer defer it, execute it now.
        LOG_DEBUG("Keymap: [%u] execute immediate %s\n", slot->index(), press_or_release);
    }

    rgb_thread::obj().signal_key_event(slot, is_press);
    is_press ? (*slot)->press(slot) : (*slot)->release(slot);
    //  Remove the deferred event from the event buffer.
    m_key_events.discard_last_deferred();
}
