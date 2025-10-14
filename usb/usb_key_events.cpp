#include "irq.h"                // for irq_enable(), irq_restore()
#include "log.h"
#include "mutex.h"              // for mutex_lock(), mutex_unlock(), ...

#include "usb_key_events.hpp"



void usb_key_events::push(key_event_t event, bool wait_if_full)
{
    // Todo: Why isn't this log displayed?
    // LOG_DEBUG("USB_HID: queue %s (0x%x %s)",
    //     press_or_release(event.is_press),
    //     event.keycode, keycode_to_name[event.keycode]);

    if ( mutex_trylock(&m_not_full) == 0 && wait_if_full ) {
        // push() is intended to execute in thread context with interrupts disabled,
        // ensuring thread safety. However, when waiting for a mutex here, interrupts
        // and other threads must be allowed to run to avoid deadlock. A condition
        // variable could be used, but enabling interrupts offers a lower-footprint
        // alternative in this context.
        unsigned state = irq_enable();
        mutex_lock(&m_not_full);  // wait until it is unlocked.
        irq_restore(state);
    }

    m_events[m_end++ & (QUEUE_SIZE - 1)] = event;
    if ( not_full() )
        // If queue is full the mutex remains locked.
        mutex_unlock(&m_not_full);
    else
        // This prevents the queue from holding more events than QUEUE_SIZE.
        m_begin = m_end - QUEUE_SIZE;
}

bool usb_key_events::pop()
{
    if ( not_empty() ) {
        m_begin++;
        mutex_unlock(&m_not_full);
        return true;
    }
    return false;
}

void usb_key_events::clear()
{
    LOG_DEBUG("USB_HID: clear key event queue");
    m_begin = m_end;
    mutex_unlock(&m_not_full);
}

bool usb_key_events::peek(key_event_t& event) const
{
    if ( not_empty() ) {
        event = m_events[m_begin & (QUEUE_SIZE - 1)];
        return true;
    }
    return false;
}
