#include "board.h"              // for THREAD_PRIO_USBHUB
#include "event.h"              // for event_queue_init(), event_get()
#include "thread.h"             // for thread_create(), thread_get_unchecked()
#include "thread_flags.h"       // for thread_flags_wait_any()

#include "usbhub_states.hpp"    // for usbhub_state::init(), ...
#include "usbhub_thread.hpp"



thread_t* usbhub_thread::m_pthread = nullptr;

// THREAD_STACKSIZE_SMALL is incompatible with printf().
char usbhub_thread::m_thread_stack[THREAD_STACKSIZE_DEFAULT];

event_queue_t usbhub_thread::m_queue;

void usbhub_thread::init()
{
    m_pthread = thread_get_unchecked( thread_create(
        m_thread_stack, sizeof(m_thread_stack),
        THREAD_PRIO_USBHUB,
        THREAD_CREATE_STACKTEST,
        _thread_entry, nullptr, "usbhub_thread") );
}

NORETURN void* usbhub_thread::_thread_entry(void*)
{
    // event_queue_init() should be called from the queue-owning thread.
    event_queue_init(&m_queue);

    // Initialize the usbhub state machine.
    usbhub_state::init();

    while ( true ) {
        // Zzz
        thread_flags_t flags = thread_flags_wait_any(
            FLAG_EVENT
            | FLAG_USB_SUSPEND
            | FLAG_USB_RESUME
            | FLAG_USBPORT_SWITCHOVER
            | FLAG_EXTRA_MANUAL
            | FLAG_EXTRA_AUTOMATIC
            | FLAG_TIMEOUT
            );

        if ( flags & FLAG_EVENT ) {
            event_t* event;
            while ( (event = event_get(&m_queue)) != nullptr )
                event->handler(event);
        }

        if ( flags & FLAG_USB_SUSPEND )
            usbhub_state::pstate->process_usb_suspend();

        if ( flags & FLAG_USB_RESUME )
            usbhub_state::pstate->process_usb_resume();

        if ( flags & FLAG_USBPORT_SWITCHOVER )
            usbhub_state::pstate->process_usbport_switchover();

        if ( flags & FLAG_EXTRA_MANUAL )
            usbhub_state::pstate->process_extra_enable_manually();

        if ( flags & FLAG_EXTRA_AUTOMATIC )
            usbhub_state::pstate->process_extra_enable_automatically();

        if ( flags & FLAG_TIMEOUT )
            usbhub_state::pstate->process_timeout();
    }
}
