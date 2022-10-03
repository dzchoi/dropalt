#include "adc_get.h"            // for adc_init() and adc_configure()
#include "thread.h"
#include "usb2422.h"            // for usbhub_init()

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "adc_input.hpp"
#include "adc_thread.hpp"
#include "usbport_state.hpp"



adc_thread::adc_thread()
{
    // Initialize ADC0, and GCLK3_48MHz using GCLK_SOURCE_DFLL.
    // Note adc_init() is not performed by periph_init() or auto_init().
    adc_init(ADC_LINE_5V);
    adc_init(ADC_LINE_CON1);
    adc_init(ADC_LINE_CON2);
    adc_configure(ADC0, ADC_RES_12BIT);

    // Todo: Why can't this move following "m_pthread = ..." below?
    // Initialize USB2422.
    usbhub_init();

    // Initialize events.
    m_event_report_host.handler = &_hdlr_report_host;
    m_event_report_extra.handler = &_hdlr_report_extra;

    const kernel_pid_t pid = thread_create(
        m_stack, sizeof(m_stack),
        THREAD_PRIO_ADC,
        THREAD_CREATE_STACKTEST,
        _adc_thread, this, "adc_thread");

    m_pthread = thread_get(pid);
}

void* adc_thread::_adc_thread(void* arg)
{
    adc_thread* const that = static_cast<adc_thread*>(arg);

    // The event_queue_init() should be called from the queue-owning thread.
    event_queue_init(&that->m_queue);

    // Not to be called from the constructor to avoid mutual recursion with adc_input().
    adc_input::wait_for_stable_5v();

    // Keep measuring v_5v always.
    adc_input::v_5v.schedule_periodic();

    // Not to be called from the constructor to avoid mutual recursion with adc_input().
    // Run the initial state, which can start a timer for FLAG_TIMEOUT.
    usbport::init_state();

    while ( true ) {
        // Zzz
        thread_flags_t flags = thread_flags_wait_any(
            FLAG_EVENT
            | FLAG_USBHUB_ACTIVE
            | FLAG_USBHUB_SWITCHOVER
            | FLAG_TIMEOUT
            );

        // Note that most event signals (e.g. measure/report events) are repetitive but
        // others are not. If a signal is not processed by the current state it is lost
        // and not passed over to the next state.

        if ( flags & FLAG_EVENT ) {
            event_t* event;
            while ( (event = event_get(&that->m_queue)) != nullptr )
                // The handler() will call an appropriate process_*() on usbport::pstate.
                event->handler(event);
        }

        if ( flags & FLAG_USBHUB_ACTIVE ) {
            usbport::pstate->process_usbhub_active();
        }

        if ( flags & FLAG_USBHUB_SWITCHOVER ) {
            usbport::pstate->process_usbhub_switchover();
        }

        if ( flags & FLAG_TIMEOUT ) {
            usbport::pstate->process_timeout();
        }
    }

    // Should never reach this point.
    adc_input::v_5v.schedule_cancel();
    return nullptr;
}

void adc_thread::_hdlr_report_host(event_t* event)
{
    (void)event;
    // Todo: Try to adjust GCR first before reporting to pstate.
    usbport::pstate->process_v_5v_level();
}

void adc_thread::_hdlr_report_extra(event_t* event)
{
    (void)event;
    // It is always that usbport::v_extra != nullptr as this method is called only when
    // v_extra->schedule_periodic() is executed.
    if ( usbport::v_extra->is_connected_level() )
        usbport::pstate->process_extra_connected();
    else
        usbport::pstate->process_extra_unconnected();
}
