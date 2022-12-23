#pragma once

#include "board.h"              // for ADC_LINE_*
#include "mutex.h"
#include "ztimer.h"             // for ztimer_t, ztimer_set() and ztimer_remove()

#include <atomic>               // for std::atomic<>
#include "event_ext.hpp"        // for event_ext_t
#include "features.hpp"         // for RGB_GCR_CHANGE_PERIOD_MS, ...



class adc_thread;
class adc_input_v_5v;
class adc_input_v_con;



class adc_input {
public:
    static adc_input_v_5v v_5v;
    static adc_input_v_con v_con1;
    static adc_input_v_con v_con2;

    const uint8_t line;

    // Read the most recent result.
    uint16_t read() const { return m_result; }

    // Schedule periodic measurements, which will be performed on adc_thread context.
    void schedule_periodic() {
        ztimer_set(ZTIMER_MSEC, &m_schedule_timer, MEASURE_PERIOD_MS);
    }

    // Cancel the schedule.
    void schedule_cancel() {
        ztimer_remove(ZTIMER_MSEC, &m_schedule_timer);
    }

    adc_input(const adc_input&) =delete;
    adc_input& operator=(const adc_input&) =delete;

protected:
    // To be used only for creating v_5v, v_con1 and v_con2.
    adc_input(uint8_t line);

    // Start measuring the input (non-blocking). The result will be reported later to
    // adc_thread, calling signal_report_5v() or signal_report_extra().
    void async_measure();

    // Measure the input (blocking). Though blocking the caller it is not spinning but
    // waiting for the result using mutex. It can be intermixed with async_measure().
    // The result can be read immediately after using read(). It does not report the
    // result to adc_thread. So it can be called without having adc_thread initialized
    // (e.g. from the constructor of adc_thread).
    void sync_measure();

private:
    mutex_t m_mutex;

    // Declared atomic, as this can be updated from isr without notice.
    std::atomic<uint16_t> m_result = 0u;

    // Signal to adc_thread that result is ready.
    virtual void _isr_signal_report() =0;

    // Measure v_5v at every 16 ms and v_con1/con2 at every 5 ms.
    const uint32_t MEASURE_PERIOD_MS = (line == ADC_LINE_5V)
        ? RGB_GCR_CHANGE_PERIOD_MS : EXTRA_PORT_MEASURING_PERIOD_MS;

    ztimer_t m_schedule_timer = {
        .base = {},
        .callback = &_tmo_periodic_measure,
        .arg = this
    };
    static void _tmo_periodic_measure(void* arg);

    // Define a separate event for measuring each adc_input, because sharing a single
    // event struct with different port numbers as argument would lose early events that
    // are already in the event queue when a new event is pushed. As to reporting, we use
    // thread signals instead of events, FLAG_REPORT_5V and FLAG_REPORT_EXTRA defined in
    // adc_thread.
    event_ext_t<adc_input*> m_event_periodic_measure;
    static void _hdlr_periodic_measure(event_t* event);
};



enum v_5v_level: int8_t {
    V_5V_PANIC      = -1,
    V_5V_UNSTABLE   = 0,
    V_5V_LOW        = 1,
    V_5V_STABLE     = V_5V_LOW,
    V_5V_MID        = 2,
    V_5V_HIGH       = 3
};

class adc_input_v_5v: public adc_input {
public:
    adc_input_v_5v& sync_measure();

    // Read the result in level.
    v_5v_level level() const { return m_level; }

    // Wait (indefinitely) until v_5v stays above ADC_5V_START_LEVEL for around 5 ms,
    // blocking the calling thread. It can cause a watchdog reset if it waits too long.
    void wait_for_stable_5v();

private:
    // Can be constructed only from adc_input class.
    friend class adc_input;
    adc_input_v_5v(): adc_input(ADC_LINE_5V) {}

    // m_level is updated according to m_result on sync_measure() and async_measure().
    v_5v_level m_level = V_5V_UNSTABLE;

    void update_level();

    void _isr_signal_report();
};



class adc_input_v_con: public adc_input {
public:
    adc_input_v_con& sync_measure() { adc_input::sync_measure(); return *this; }

    // To be used only on the port which has SR_CTRL_SRC_x disabled (i.e. v_extra).
    bool is_device_connected() const;

    // This can be used when the port has SR_CTRL_SRC_x either enabled or disabled.
    // (However, some old version of PCB seems not working when SR_CTRL_SRC_x is enabled.
    // So, it would be safer to have SR_CTRL_SRC_x disabled.)
    bool is_host_connected() const;

private:
    // Can be constructed only from adc_input class.
    friend class adc_input;
    using adc_input::adc_input;

    void _isr_signal_report();

    const uint16_t NOMINAL_LEVEL =
        line == ADC_LINE_CON1 ? ADC_CON1_NOMINAL : ADC_CON2_NOMINAL;
};
