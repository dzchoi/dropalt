#include "usb2422.h"            // for USB_PORT_*

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "adc_input.hpp"
#include "adc_thread.hpp"



adc_input_v_5v adc_input::v_5v;
adc_input_v_con adc_input::v_con1 { ADC_LINE_CON1 };
adc_input_v_con adc_input::v_con2 { ADC_LINE_CON2 };

adc_input::adc_input(uint8_t line): line(line)
{
    mutex_init(&m_mutex);

    m_event_periodic_measure.handler = &_hdlr_periodic_measure;
    m_event_periodic_measure.arg = this;
}

void adc_input::wait_for_stable_5v()
{
    constexpr int V_5V_STABILITY_COUNT = 5;

    int repeat = 0;
    int count = 0;
    while ( repeat++ < V_5V_STABILITY_COUNT ) {
        (void)v_5v.sync_measure();
        if ( v_5v.level() < adc_input_v_5v::V_5V_STABLE )
            repeat = 0;
        ++count;
    }
    DEBUG("ADC: v_5v stabilized in %d ms\n", count);
}

uint8_t adc_input::measure_host_port()
{
    const uint16_t v1 = v_con1.sync_measure();
    const uint16_t v2 = v_con2.sync_measure();
    DEBUG("ADC: v_con1=%d v_con2=%d\n", v1, v2);
    return v1 >= v2 ? USB_PORT_1 : USB_PORT_2;
}

void adc_input::_isr_get_result(void* arg, uint16_t result)
{
    adc_input* const that = static_cast<adc_input*>(arg);
    that->m_result = result;
    mutex_unlock(&that->m_mutex);
    that->_signal_report();
}

void adc_input_v_5v::_signal_report() const
{
    adc_thread::obj().signal_report_host();
}

void adc_input_v_con::_signal_report() const
{
    adc_thread::obj().signal_report_extra();
}

void adc_input::_tmo_periodic_measure(adc_input* that)
{
    // As being executed in interrupt context, we cannot call async_measure()
    // directly, but instead we delegate the actual (start of) measurement to
    // adc_thread.
    adc_thread::obj().signal_event(&that->m_event_periodic_measure);
};

void adc_input::_hdlr_periodic_measure(event_t* event)
{
    adc_input* const that = static_cast<event_ext_t<adc_input*>*>(event)->arg;
    that->async_measure();
}

adc_input_v_5v::v_5v_level adc_input_v_5v::level() const
{
    const uint16_t result = read();

    if ( result >= ADC_5V_HIGH ) return V_5V_HIGH;
    if ( result >= ADC_5V_LOW ) return V_5V_MID;
    if ( result >= ADC_5V_START_LEVEL ) return V_5V_LOW;
    if ( result >= ADC_5V_PANIC ) return V_5V_UNSTABLE;
    return V_5V_PANIC;
}
