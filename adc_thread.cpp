#include "adc_get.h"
#include "board.h"
#include "thread.h"
#include "usb2422.h"            // for usbhub_configure() and usbhub_active()

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "adc_input.hpp"        // for v_5v and v_conx.
#include "adc_thread.hpp"



adc_thread::adc_thread()
{
    // Initialize ADC0, and GCLK3_48MHz using GCLK_SOURCE_DFLL.
    // Note adc_init() is not performed by periph_init() or auto_init().
    adc_init(ADC_LINE_5V);
    adc_init(ADC_LINE_CON1);
    adc_init(ADC_LINE_CON2);
    adc_configure(ADC0, ADC_RES_12BIT);

    // Initialize events.
    m_event_to_select_host_port.handler = &_hdlr_to_select_host_port;
    m_event_to_select_host_port.arg0 = this;

    const kernel_pid_t pid = thread_create(
        m_stack, sizeof(m_stack),
        THREAD_PRIORITY_ADC,
        THREAD_CREATE_STACKTEST,
        _adc_thread, this, "adc_thread");

    m_pthread = thread_get(pid);
}

void* adc_thread::_adc_thread(void* arg)
{
    adc_thread* const that = static_cast<adc_thread*>(arg);
    // The event_queue_init() should be called from the queue-owning thread.
    event_queue_init(&that->m_queue);
    // Zzz
    event_loop(&that->m_queue);
    return nullptr;
}

void adc_thread::wait_for_stable_5v()
{
    constexpr int V_5V_STABILITY_COUNT = 5;

    int count = 0;
    while ( count++ < V_5V_STABILITY_COUNT ) {
        if ( v_5v.sync_measure() < ADC_5V_START_LEVEL )
            count = 0;
    }
}

void adc_thread::_hdlr_to_select_host_port(event_t* event)
{
    adc_thread* const that =
        static_cast<decltype(m_event_to_select_host_port)*>(event)->arg0;
    uint8_t desired_port =
        static_cast<decltype(m_event_to_select_host_port)*>(event)->arg1;

    that->m_usb_host_port = USB_PORT_UNKNOWN;  // to be determined below
    that->m_usb_extra_state = USB_EXTRA_STATE_DISABLED;
    that->m_usb_extra_manual = false;

    constexpr uint32_t RETRY_TIMER_US = 2 *US_PER_SEC;
    uint32_t time_to_retry = xtimer_now_usec();

    do {
        const uint32_t now = xtimer_now_usec();
        if ( (int32_t)(now - time_to_retry) >= 0 ) {
            // Indicate on the LED0 that we're alive. We can stay in this loop when
            // plugged in to a suspended host, until the host wakes up.
            LED0_ON;
            usbhub_disable_all_ports();

            if ( desired_port == USB_PORT_UNKNOWN )
                // Determine host by comparing voltages on ADC_LINE_CON1 and CON2.
                desired_port = determine_host_port();
            usbhub_enable_host_port(desired_port);

            // When the keyboard is powered up by manually plugging in to a host port
            // instead of powering up the host with the keyboard already connected,
            // usbhub_configure() can fail sometimes. HUB does not respond to the
            // configuration data sent over I2C (until the voltage or clock is stable?).
            while ( !usbhub_configure() )
                xtimer_msleep(100);

            // If the given desired port cannot be acquired first, try either port that
            // has a higher voltage afterwards.
            desired_port = USB_PORT_UNKNOWN;
            time_to_retry = now + RETRY_TIMER_US;
            LED0_OFF;
        }

        xtimer_msleep(1);
    } while ( !usbhub_active() );  // Check usbhub_active() at every 1 ms.

    that->m_usb_host_port = usbhub_host_port();

    // DEBUG seems causing a crash at this early stage of power-up.
    // DEBUG("ADC: determined host port @%d\n", that->m_usb_host_port);
}

void adc_thread::change_extra_port_state(usb_extra_state_t state)
{
    usbhub_enable_disable_extra_port(other(m_usb_host_port), state == USB_EXTRA_STATE_ENABLED);

    switch ( state ) {
        case USB_EXTRA_STATE_FORCED_DISABLED:
            DEBUG("USB_HUB: extra port forced disabled\n");
            break;

        case USB_EXTRA_STATE_DISABLED:
            DEBUG("USB_HUB: extra port disabled\n");
            break;

        case USB_EXTRA_STATE_ENABLED:
            DEBUG("USB_HUB: extra port enabled\n");
            break;
    }

    m_usb_extra_state = state;
}

void adc_thread::handle_extra_device(bool is_extra_plugged_in)
{
    // As it can be called (periodically) from the start of "adc_thread", we need to
    // ensure that usbhub_wait_for_host() is done before.
    if ( m_usb_host_port == USB_PORT_UNKNOWN )
        return;

    switch ( m_usb_extra_state ) {
        case USB_EXTRA_STATE_FORCED_DISABLED:
            // Detect unplug and reset state to disabled
            if ( !is_extra_plugged_in )
                m_usb_extra_state = USB_EXTRA_STATE_DISABLED;
            m_usb_extra_manual = false;
            break;  // Do nothing even if unplug detected

        case USB_EXTRA_STATE_DISABLED:
            if ( is_extra_plugged_in || m_usb_extra_manual )
                change_extra_port_state(USB_EXTRA_STATE_ENABLED);
            break;

        case USB_EXTRA_STATE_ENABLED:
            if ( !is_extra_plugged_in && !m_usb_extra_manual )
                change_extra_port_state(USB_EXTRA_STATE_DISABLED);
            break;
    }
}
