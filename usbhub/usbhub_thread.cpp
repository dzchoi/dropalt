#include "adc_get.h"            // for adc_init() and adc_configure()
#include "board.h"              // for ADC_LINE_*
#include "event.h"              // for event_queue_init(), event_get()
#include "sr_exp.h"             // for sr_exp_init()
#include "thread.h"             // for thread_create(), thread_get_unchecked()
#include "thread_flags.h"       // for thread_flags_wait_any()
#include "usb2422.h"            // for usbhub_init()

#include "adc.hpp"              // for adc::init(), wait_for_stable_5v(), ...
#include "usbhub_states.hpp"    // for usbhub_state::init(), ...
#include "usbhub_thread.hpp"



thread_t* usbhub_thread::m_pthread = nullptr;

// THREAD_STACKSIZE_SMALL is incompatible with printf().
char usbhub_thread::m_thread_stack[THREAD_STACKSIZE_DEFAULT];

event_queue_t usbhub_thread::m_queue;

void usbhub_thread::init()
{
    // Initialize Shift Register.
    sr_exp_init();

    adc::init();

    adc::v_5v.wait_for_stable_5v();

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

    // Initialize USB2422.
    // Could be called from usbhub_thread::init(), but usbhub_init() will take a while
    // and we may better call it from thread context.
    usbhub_init();

    // Initialize the usbhub state machine.
    usbhub_state::init();

    // Continuously monitor the v_5v.
    adc::v_5v.schedule_periodic();

    while ( true ) {
        // Zzz
        thread_flags_t flags = thread_flags_wait_any(
            FLAG_EVENT
            | FLAG_USB_SUSPEND
            | FLAG_USB_RESUME
            | FLAG_USBPORT_SWITCHOVER
            | FLAG_REPORT_V_5V
            | FLAG_REPORT_V_CON
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

        if ( flags & FLAG_REPORT_V_5V )
            usbhub_state::pstate->process_v_5v_report();

        if ( flags & FLAG_REPORT_V_CON )
            usbhub_state::pstate->process_v_con_report();

        if ( flags & FLAG_EXTRA_MANUAL )
            usbhub_state::pstate->process_extra_enable_manually();

        if ( flags & FLAG_EXTRA_AUTOMATIC )
            usbhub_state::pstate->process_extra_enable_automatically();

        if ( flags & FLAG_TIMEOUT )
            usbhub_state::pstate->process_timeout();
    }

    // Should never reach this point.
    // adc::v_5v.schedule_cancel();
}
