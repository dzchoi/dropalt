#include "adc_get.h"
#include "board.h"
#include "periph/wdt.h"
#include "thread.h"
#include "xtimer.h"

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "adc_thread.hpp"



adc_thread::adc_thread()
{
    // Initialize ADC0, and GCLK3_48MHz using GCLK_SOURCE_DFLL.
    // Note adc_init() is not performed by periph_init() or auto_init().
    adc_init(ADC_LINE_5V);
    adc_init(ADC_LINE_CON1);
    adc_init(ADC_LINE_CON2);
    adc_configure(ADC0, ADC_RES_12BIT);

    // The created thread will not start until signaled with
    // ADC_THREAD_FLAG_V_*_SCAN_NEXT.
    const kernel_pid_t pid = thread_create(
        m_stack, sizeof(m_stack),
        THREAD_PRIORITY_ADC,
        THREAD_CREATE_STACKTEST | THREAD_CREATE_SLEEPING,
        _adc_thread, nullptr, "adc_thread");

    m_pthread = thread_get(pid);
}

void* adc_thread::_adc_thread(void* arg)
{
    while ( true ) {}
    return arg;
}

void adc_thread::wait_for_stable_5v()
{
    constexpr int V_5V_STABILITY_COUNT = 5;  // amount to ~10ms of spinning.

    int count = 0;
    while ( count++ < V_5V_STABILITY_COUNT ) {
        xtimer_msleep(1);
        m_v_5v = adc_get(ADC_LINE_5V, nullptr);
        if ( m_v_5v < ADC_5V_START_LEVEL )
            count = 0;
    }
}
