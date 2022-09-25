#pragma once

#include "event.h"
#include "thread.h"



class adc_thread {
public:
    static adc_thread& obj() {
        static adc_thread obj;
        return obj;
    }

    // Signal a (generic) event to adc_thread.
    void signal_event(event_t* event) { event_post(&m_queue, event); }

    // Signal that host report is ready.
    void signal_report_host() { event_post(&m_queue, &m_event_report_host); }

    // Signal that extra report is ready.
    void signal_report_extra() { event_post(&m_queue, &m_event_report_extra); }

    adc_thread(const adc_thread&) =delete;
    void operator=(const adc_thread&) =delete;

private:
    thread_t* m_pthread;

    // THREAD_STACKSIZE_TINY cannot be used with printf().
    char m_stack[THREAD_STACKSIZE_SMALL];

    event_queue_t m_queue;  // event queue

    adc_thread();  // Can be called only by obj().

    // thread body
    static void* _adc_thread(void* arg);

    static void _isr_usbhub_active(void* arg);

    event_t m_event_report_host;
    static void _hdlr_report_host(event_t* event);

    event_t m_event_report_extra;
    static void _hdlr_report_extra(event_t* event);

    enum {
        FLAG_EVENT              = 0x0001,  // == THREAD_FLAG_EVENT from event.h
        FLAG_USBHUB_ACTIVE      = 0x0002,
        FLAG_USBHUB_SWITCHOVER  = 0x0004,
        FLAG_TIMEOUT            = THREAD_FLAG_TIMEOUT  // (1u << 14)
    };
};
