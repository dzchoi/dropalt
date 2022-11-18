#include "adc_get.h"            // for adc_get()
#include "usb2422.h"            // for usbhub_is_configured_for_host_port()
#include "xtimer.h"             // for xtimer_now_usec()

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "adc_input.hpp"
#include "adc_thread.hpp"       // for signal_report_5v/extra() and signal_event()



adc_input_v_5v adc_input::v_5v;
adc_input_v_con adc_input::v_con1 { ADC_LINE_CON1 };
adc_input_v_con adc_input::v_con2 { ADC_LINE_CON2 };



adc_input::adc_input(uint8_t line): line(line)
{
    mutex_init(&m_mutex);

    m_event_periodic_measure.handler = &_hdlr_periodic_measure;
    m_event_periodic_measure.arg = this;
}

void adc_input::async_measure()
{
    mutex_lock(&m_mutex);
    adc_get(line,
        [](void* arg, uint16_t result) {
            adc_input* const that = static_cast<adc_input*>(arg);
            that->m_result = result;
            mutex_unlock(&that->m_mutex);
            that->_isr_signal_report();
        },
        this);
}

void adc_input::sync_measure()
{
    mutex_lock(&m_mutex);
    adc_get(line,
        [](void* arg, uint16_t result) {
            adc_input* const that = static_cast<adc_input*>(arg);
            that->m_result = result;
            mutex_unlock(&that->m_mutex);
        },
        this);

    mutex_lock(&m_mutex);  // Wait for the result to arrive.
    mutex_unlock(&m_mutex);
}

void adc_input_v_5v::_isr_signal_report() const
{
    adc_thread::obj().signal_report_5v();
}

void adc_input_v_con::_isr_signal_report() const
{
    adc_thread::obj().signal_report_extra();
}

void adc_input::_tmo_periodic_measure(adc_input* that)
{
    // As executed in interrupt context, we cannot call async_measure() directly but
    // instead we delegate the actual (start of) measurement to adc_thread.
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

void adc_input_v_5v::wait_for_stable_5v()
{
    constexpr int V_5V_STABILITY_COUNT = 5;

#if ENABLE_DEBUG
    const uint32_t since = xtimer_now_usec();
#endif

    int repeat = 0;
    while ( repeat++ < V_5V_STABILITY_COUNT )
        if ( sync_measure().level() < V_5V_STABLE )
            repeat = 0;

    DEBUG("ADC: v_5v stabilized in %lu us\n", xtimer_now_usec() - since);
}

bool adc_input_v_con::is_device_connected() const
{
    assert( !usbhub_is_configured_for_host_port(line) );
    return read() < NOMINAL_LEVEL - ADC_CON_NOMINAL_CHANGE_THR;
}

bool adc_input_v_con::is_host_connected() const
{
    return read() >=
        (usbhub_is_configured_for_host_port(line)
            ? ADC_CON_HOST_CONNECTED : NOMINAL_LEVEL + ADC_CON_NOMINAL_CHANGE_THR);
}
