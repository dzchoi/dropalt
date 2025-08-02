#pragma once

#include "board.h"              // for ADC_LINE_*
#include "mutex.h"              // for mutex_init()
#include "ztimer.h"             // for ztimer_set(), ztimer_remove()

#include <atomic>               // for std::atomic<>
#include "event_ext.hpp"        // for event_ext_t
#include "config.hpp"            // for RGB_GCR_CHANGE_PERIOD_MS, ...



class adc_v_5v;
class adc_v_con;

class adc {
public:
    // These are the only instances of adc class.
    static adc_v_5v v_5v;
    static adc_v_con v_con1;
    static adc_v_con v_con2;

    static void init();

    // ADC line to measure
    const uint8_t line;

    adc(const adc&) =delete;
    adc& operator=(const adc&) =delete;

    // Read the latest ADC result.
    uint16_t read() const { return m_result; }

    // Schedule periodic measurement, which will be performed in usbhub_thread context.
    void schedule_periodic() {
        ztimer_set(ZTIMER_MSEC, &m_schedule_timer, MEASURE_PERIOD_MS);
    }

    // Cancel the schedule.
    void schedule_cancel() {
        ztimer_remove(ZTIMER_MSEC, &m_schedule_timer);
    }

protected:
    // To be used only for creating v_5v, v_con1, and v_con2.
    adc(uint8_t line): line(line) { mutex_init(&m_lock); }

    // Start measuring the ADC line (non-blocking). The result will be reported later to
    // usbhub_thread, calling signal_v_5v_report() or signal_v_con_report().
    void async_measure();

    // Measure the ADC line now (blocking), waiting for a result from the ADC interrupt.
    // The result can be read using read(). It can be used alongside async_measure().
    // Note: this method does not report the result to usbhub_thread, thus allowing it to
    // be called before initializing usbhub_thread.
    void _sync_measure();

private:
    // Each of v_5v, v_con1 and v_con2 will have its own mutex.
    mutex_t m_lock;

    // Declared as atomic to ensure thread-safe updates from ISR.
    std::atomic<uint16_t> m_result = 0;

    // Signal to usbhub_thread that result is ready.
    virtual void _isr_signal_report() =0;

    // Measure v_5v every RGB_GCR_CHANGE_PERIOD_MS and v_con1/con2 every
    // EXTRA_PORT_MEASURING_PERIOD_MS.
    const uint32_t MEASURE_PERIOD_MS =
        line == ADC_LINE_5V ? RGB_GCR_CHANGE_PERIOD_MS : EXTRA_PORT_MEASURING_PERIOD_MS;

    ztimer_t m_schedule_timer = { {}, &_tmo_periodic_measure, this };
    static void _tmo_periodic_measure(void* arg);

    // Each `adc` has its own event struct for measurement. Sharing a single event struct
    // with different port numbers as arguments could result in losing early events
    // already in the event queue when a new event is added. For reporting, however,
    // thread signals are used instead of events, specifically FLAG_REPORT_V_5V and
    // FLAG_REPORT_V_CON defined in usbhub_thread.
    event_ext_t<adc*> m_event_periodic_measure = {
        nullptr, &_hdlr_periodic_measure, this
    };
    static void _hdlr_periodic_measure(event_t* event);
};



class adc_v_5v: public adc {
public:
    enum v_5v_level: int8_t {
        PANIC       = -1,
        UNSTABLE    = 0,
        LOW         = 1,
        STABLE      = LOW,
        MID         = 2,
        HIGH        = 3
    };

    adc_v_5v& sync_measure() {
        _sync_measure();
        update_level();
        return *this;
    }

    // Read the result in level.
    v_5v_level level() const { return m_level; }

    // Wait (indefinitely) until v_5v stays above ADC_5V_START_LEVEL for around 5 ms,
    // blocking the calling thread. It can cause a watchdog reset if it waits too long.
    void wait_for_stable_5v();

private:
    // Can be constructed only from adc class.
    friend class adc;
    adc_v_5v(): adc(ADC_LINE_5V) {}

    // m_level is determined based on m_result obtained from sync_measure() and
    // async_measure().
    v_5v_level m_level = UNSTABLE;

    void update_level();

    void _isr_signal_report();
};



class adc_v_con: public adc {
public:
    adc_v_con& sync_measure() {
        _sync_measure();
        return *this;
    }

    // To be used only on the port which has SR_CTRL_SRC_x disabled (i.e. v_extra).
    bool is_device_connected() const;

    // This can be used on the port regardless of whether SR_CTRL_SRC_x is enabled 
    // (i.e. v_host) or disabled (i.e. v_extra). However, it may not work correctly if
    // the USB port on the keyboard is physically damaged, which could result in false
    // always.
    bool is_host_connected() const;

private:
    // Can be constructed only from adc class.
    using adc::adc;

    void _isr_signal_report();

    const uint16_t NOMINAL_LEVEL =
        line == ADC_LINE_CON1 ? ADC_CON1_NOMINAL : ADC_CON2_NOMINAL;
};
