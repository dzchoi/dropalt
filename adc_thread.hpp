#pragma once

#include "event.h"
#include "thread.h"
#include "usb2422.h"

#include "adc_input.hpp"
#include "features.hpp"



class adc_thread {
public:
    static adc_thread& instance() {
        static adc_thread instance;
        return instance;
    }

    // Wait (indefinitely) until v_5v stays above ADC_5V_START_LEVEL for around 5 ms,
    // blocking the calling thread, and may cause a watchdog reset if it waits too long.
    // It also sets up the periodic measurement afterwards for ADC_LINE_5V.
    void wait_for_stable_5v();

    // Check the desired_port and attach the Hub to the port. If the desired_port is not
    // given either ADC_LINE_CON1 or CON2 is tried.
    // It also sets up the periodic measurements afterwards for ADC_LINE_CON1 and CON2.
    // Todo: Automatic calling of usbhub_wait_for_host(),
    //   - when the current v_con (not v_extra) is down (i.e. in panic mode), or
    //   - when USB is disconnected (usbus.state == USBUS_STATE_DISCONNECT, but not
    //     working for now), or
    // Todo: If the USB in the desired port is suspended when switchover, wake it up first.
    void async_select_host_port(uint8_t desired_port =USB_PORT_UNKNOWN) {
        m_event_to_select_host_port.arg1 = desired_port;
        event_post(&m_queue, &m_event_to_select_host_port);
    }

    // Signal an event to adc_thread.
    void signal_event(event_t* event) {
        event_post(&m_queue, event);
    }

    adc_thread(const adc_thread&) =delete;
    adc_thread& operator=(const adc_thread&) =delete;

private:
    adc_thread();

    static constexpr size_t THREAD_PRIORITY_ADC = THREAD_PRIORITY_MAIN - 1;

    // thread body
    static void* _adc_thread(void* arg);

    thread_t* m_pthread;
    char m_stack[THREAD_STACKSIZE_TINY];

    event_queue_t m_queue;

    // Measure ADC_LINE_CON1/2 (blocking) and return the one connected to the host.
    static uint8_t determine_host_port() {
        return v_con1.sync_measure() >= v_con2.sync_measure()
               ? USB_PORT_1 : USB_PORT_2;
    }

    event_ext_t<adc_thread*, uint8_t> m_event_to_select_host_port;
    static void _hdlr_to_select_host_port(event_t* event);

    enum usb_extra_state_t: int_fast8_t {
        USB_EXTRA_STATE_FORCED_DISABLED = -1,
        USB_EXTRA_STATE_DISABLED        = 0,
        USB_EXTRA_STATE_ENABLED         = 1
    };

    uint8_t m_usb_host_port = USB_PORT_UNKNOWN;
    usb_extra_state_t m_usb_extra_state = USB_EXTRA_STATE_DISABLED;
    bool m_usb_extra_manual = false;

    void change_extra_port_state(usb_extra_state_t state);
    void handle_extra_device(bool is_extra_plugged_in);
};
