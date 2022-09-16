#include "adc_input.hpp"
#include "adc_thread.hpp"       // for signal_event()



void adc_input::async_measure()
{
    mutex_lock(&m_mutex);
    adc_get(line, _isr_get_result, this);
    // Schedule to execute _tmo_delayed_measure() later in adc_thread context.
    xtimer_set(&m_schedule_timer, measure_period_us[line]);
}

uint16_t adc_input::sync_measure()
{
    xtimer_remove(&m_schedule_timer);  // Unschedule the _tmo_delayed_measure.
    async_measure();
    mutex_lock(&m_mutex);  // Wait for the result to arrive.
    mutex_unlock(&m_mutex);
    m_result0 = read();  // This works as m_result is atomic.
    return m_result0;
}

void adc_input::_tmo_delayed_measure(void* arg)
{
    // As being executed in interrupt context, we cannot call async_measure()
    // directly, but instead we delegate the actual (start of) measurement to
    // adc_thread.
    adc_input* const that = static_cast<adc_input*>(arg);
    adc_thread::instance().signal_event(&that->m_event_to_measure);
};

void adc_input::_isr_get_result(void* arg, uint16_t result)
{
    adc_input* const that = static_cast<adc_input*>(arg);
    that->m_result = result;
    mutex_unlock(&that->m_mutex);
}

adc_input adc_line[ADC_NUMOF] = { adc_input(0), adc_input(1), adc_input(2) };

adc_input& v_5v = adc_line[ADC_LINE_5V];
adc_input& v_con1 = adc_line[ADC_LINE_CON1];
adc_input& v_con2 = adc_line[ADC_LINE_CON2];
