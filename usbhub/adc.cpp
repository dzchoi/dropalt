#include "adc_get.h"            // for adc_init(), adc_configure(), adc_get()
#include "assert.h"
#include "log.h"
#include "mutex.h"              // for mutex_lock(), mutex_unlock()
#include "usb2422.h"            // for usbhub_is_configured_for_host_port()
#include "ztimer.h"             // for ztimer_acquire(), ztimer_now(), ...

#include "adc.hpp"
#include "usbhub_thread.hpp"    // for signal_event(), isr_process_v_5v_report(), ...



adc_v_5v adc::v_5v;
adc_v_con adc::v_con1(ADC_LINE_CON1);
adc_v_con adc::v_con2(ADC_LINE_CON2);

void adc::init()
{
    // Initialize ADC0, and GCLK3_48MHz using GCLK_SOURCE_DFLL.
    // Note: adc_init() is not called by periph_init() or auto_init().
    adc_init(ADC_LINE_5V);
    adc_init(ADC_LINE_CON1);
    adc_init(ADC_LINE_CON2);
    adc_configure(ADC0, ADC_RES_12BIT);

    v_5v.wait_for_stable_5v();
}

void adc::async_measure()
{
    mutex_lock(&m_lock);
    adc_get(line,
        [](void* arg, uint16_t result) {
            adc* const that = static_cast<adc*>(arg);
            that->m_result = result;
            mutex_unlock(&that->m_lock);
            that->isr_signal_report();
        },
        this);
}

void adc::_sync_measure()
{
    mutex_lock(&m_lock);
    adc_get(line,
        [](void* arg, uint16_t result) {
            adc* const that = static_cast<adc*>(arg);
            that->m_result = result;
            mutex_unlock(&that->m_lock);
        },
        this);

    mutex_lock(&m_lock);  // Wait for the result to arrive.
    mutex_unlock(&m_lock);
}

void adc_v_5v::isr_signal_report()
{
    update_level();
    usbhub_thread::isr_process_v_5v_report();
}

void adc_v_con::isr_signal_report()
{
    usbhub_thread::isr_process_v_con_report();
}

void adc::_tmo_periodic_measure(void* arg)
{
    adc* const that = static_cast<adc*>(arg);
    that->schedule_periodic();
    // As executed in interrupt context, we cannot call async_measure() directly.
    // Instead, we delegate the actual (starting of) measurement to usbhub_thread.
    usbhub_thread::signal_event(&that->m_event_periodic_measure);
};

void adc::_hdlr_periodic_measure(event_t* event)
{
    adc* const that = static_cast<event_ext_t<adc*>*>(event)->arg;
    that->async_measure();
}

void adc_v_5v::update_level()
{
    const uint16_t result = read();

    if ( result >= ADC_5V_HIGH )
        m_level = HIGH;
    else if ( result >= ADC_5V_LOW )
        m_level = MID;
    else if ( result >= ADC_5V_START_LEVEL )
        m_level = LOW;
    else if ( result >= ADC_5V_PANIC )
        m_level = UNSTABLE;
    else
        m_level = PANIC;
}

void adc_v_5v::wait_for_stable_5v()
{
    ztimer_acquire(ZTIMER_MSEC);
    const uint32_t since = ztimer_now(ZTIMER_MSEC);

    constexpr int V_5V_STABILITY_COUNT = 5;
    int repeat = 0;
    while ( repeat++ < V_5V_STABILITY_COUNT )
        if ( sync_measure().level() < STABLE )
            repeat = 0;

    LOG_DEBUG("ADC: v_5v stabilized in %lu ms", ztimer_now(ZTIMER_MSEC) - since);
    ztimer_release(ZTIMER_MSEC);
}

bool adc_v_con::is_device_connected() const
{
    assert( !usbhub_is_configured_for_host_port(line) );
    return read() < NOMINAL_LEVEL - ADC_CON_NOMINAL_CHANGE_THR;
}

bool adc_v_con::is_host_connected() const
{
    return read() >=
        (usbhub_is_configured_for_host_port(line)
            ? ADC_CON_HOST_CONNECTED : NOMINAL_LEVEL + ADC_CON_NOMINAL_CHANGE_THR);
}
