#include "adc_get.h"            // for adc_init() and adc_configure()
#include "thread.h"
#include "usb2422.h"            // for usbhub_init()

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "adc_input.hpp"
#include "adc_thread.hpp"
#include "usbport_states.hpp"



adc_thread::adc_thread()
{
    // Initialize ADC0, and GCLK3_48MHz using GCLK_SOURCE_DFLL.
    // Note adc_init() is not performed by periph_init() or auto_init().
    adc_init(ADC_LINE_5V);
    adc_init(ADC_LINE_CON1);
    adc_init(ADC_LINE_CON2);
    adc_configure(ADC0, ADC_RES_12BIT);

    adc_input::v_5v.wait_for_stable_5v();

    m_pthread = thread_get_unchecked( thread_create(
        m_stack, sizeof(m_stack),
        THREAD_PRIO_ADC,
        THREAD_CREATE_STACKTEST,
        _adc_thread, this, "adc_thread") );
}

void* adc_thread::_adc_thread(void* arg)
{
    adc_thread* const that = static_cast<adc_thread*>(arg);

    // The event_queue_init() should be called from the queue-owning thread.
    event_queue_init(&that->m_queue);

    // Initialize USB2422.
    usbhub_init();

    // Keep measuring v_5v at all times.
    adc_input::v_5v.schedule_periodic();

    // Set up the initial state.
    usbport::setup_state();

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
            while ( (event = event_get(&that->m_queue)) != nullptr )
                // The handler() will call an appropriate process_*() on usbport::pstate.
                event->handler(event);
        }

        if ( flags & FLAG_USB_SUSPEND )
            usbport::pstate->process_usb_suspend();

        if ( flags & FLAG_USB_RESUME )
            usbport::pstate->process_usb_resume();

        if ( flags & FLAG_USBPORT_SWITCHOVER )
            usbport::pstate->process_usbport_switchover();

        if ( flags & FLAG_REPORT_V_5V )
            usbport::pstate->process_v_5v();

        if ( flags & FLAG_REPORT_V_CON )
            usbport::pstate->process_v_con();

        if ( flags & FLAG_EXTRA_MANUAL )
            usbport::pstate->process_extra_enable_manually();

        if ( flags & FLAG_EXTRA_AUTOMATIC )
            usbport::pstate->process_extra_back_to_automatic();

        if ( flags & FLAG_TIMEOUT )
            usbport::pstate->process_timeout();
    }

    // Should never reach this point.
    adc_input::v_5v.schedule_cancel();
    return nullptr;
}
