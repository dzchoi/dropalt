#pragma once

#include "adc_get.h"
#include "event.h"
#include "mutex.h"
#include "xtimer.h"

#include <atomic>



// Extended event_t type
template <typename T, typename U =void>
struct event_ext_t: event_t {
    T arg0;
    U arg1;
};

template <typename T>
struct event_ext_t<T, void>: event_t {
    T arg;
};



class adc_input {
public:
    const adc_t line;

    adc_input(adc_t line): line(line), m_result(0) {
        mutex_init(&m_mutex);
        m_schedule_timer.callback = &_tmo_delayed_measure;
        m_schedule_timer.arg = this;
        m_event_to_measure.handler = &_hdlr_to_measure;
        m_event_to_measure.arg = this;
    }

    // Reads the most recent result.
    uint16_t read() const { return m_result; }

    // Measure the input in blocking/unblocking manner, also setting up further periodic
    // measures. (Pre: adc_thread() be created.)
    uint16_t sync_measure();
    void async_measure();

private:
    mutex_t m_mutex;
    std::atomic<uint16_t> m_result;

    // Measurement before host is determiined (for debugging purposes)
    friend int main();
    uint16_t m_result0;

    xtimer_t m_schedule_timer;
    static void _tmo_delayed_measure(void* arg);

    static void _isr_get_result(void* arg, uint16_t result);

    event_ext_t<adc_input*> m_event_to_measure;
    static void _hdlr_to_measure(event_t* event) {
        adc_input* const that = static_cast<event_ext_t<adc_input*>*>(event)->arg;
        that->async_measure();
    }

    static constexpr uint32_t measure_period_us[] = {
        11000,      // measure ADC_LINE_5V at every 11 ms
        5000, 5000  // measure ADC_LINE_CONx at every 5 ms
    };
};

extern adc_input adc_line[ADC_NUMOF];

extern adc_input& v_5v;
extern adc_input& v_con1;
extern adc_input& v_con2;
