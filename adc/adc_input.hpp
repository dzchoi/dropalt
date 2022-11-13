#pragma once

#include "adc_get.h"            // for adc_get()
#include "mutex.h"

#include <atomic>               // for std::atomic<>
#include "event_ext.hpp"        // for event_ext_t
#include "xtimer_wrapper.hpp"   // for xtimer_periodic_callback_t



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

    // Start measuring the input (non-blocking).
    void async_measure() {
        mutex_lock(&m_mutex);
        adc_get(line, _isr_get_result, this);
    }

    // Measure the input and return the result (blocking). Though blocking the caller,
    // it is not spinning, but mutex-waiting for the result.
    uint16_t sync_measure() {
        async_measure();
        mutex_lock(&m_mutex);  // Wait for the result to arrive.
        mutex_unlock(&m_mutex);
        m_result0 = read();  // This works as m_result is atomic.
        return m_result0;
    }

    // Wait (indefinitely) until v_5v stays above ADC_5V_START_LEVEL for around 5 ms,
    // blocking the calling thread, and may cause a watchdog reset if it waits too long.
    static void wait_for_stable_5v();

    // Measure v_con1 and v_con2 (blocking) and return the potential host port.
    static uint8_t measure_host_port();

    // Schedule periodic measurements which will be performed on adc_thread.
    void schedule_periodic() { m_schedule_timer.start(); }

    // Cancel the schedule.
    void schedule_cancel() { m_schedule_timer.stop(); }

    adc_input(const adc_input&) =delete;
    void operator=(const adc_input&) =delete;

protected:
    // To be used only for creating v_5v, v_con1 and v_con2.
    adc_input(uint8_t line);

private:
    mutex_t m_mutex;

    // Declared atomic, as this can be updated from isr without notice.
    std::atomic<uint16_t> m_result = 0u;

    // Measurement before host is determined (for debugging purposes)
    uint16_t m_result0 = 0u;

    static void _isr_get_result(void* arg, uint16_t result);

    // Signal to adc_thread that result is ready.
    virtual void _signal_report() const =0;

    const uint32_t MEASURE_PERIOD_US = _MEASURE_PERIODS[line];
    static constexpr uint32_t _MEASURE_PERIODS[] = {
        11000,      // measure v_5v at every 11 ms
        5000, 5000  // measure v_con1/con2 at every 5 ms
    };
    xtimer_periodic_callback_t<adc_input*> m_schedule_timer {
        MEASURE_PERIOD_US, &_tmo_periodic_measure, this };
    static void _tmo_periodic_measure(adc_input* that);

    // Define a separate event for each adc_input. Sharing a single event with the line
    // numbers parametrized would lose early events that are already in the event queue
    // when a new event is pushed. See post_event() in Riot ("If the event is already
    // queued when calling this function, the event will not be touched and remain in the
    // previous position on the queue.")
    // However, report events are provided in adc_thread as there can exist only two such
    // events, host report and extra report. No need to have three events, with one per
    // each adc_input.
    event_ext_t<adc_input*> m_event_periodic_measure;
    static void _hdlr_periodic_measure(event_t* event);
};



class adc_input_v_5v: public adc_input {
public:
    enum v_5v_level: int8_t {
        V_5V_PANIC      = -1,
        V_5V_UNSTABLE   = 0,
        V_5V_LOW        = 1,
        V_5V_STABLE     = V_5V_LOW,
        V_5V_MID        = 2,
        V_5V_HIGH       = 3
    };

    // Read the result in level.
    v_5v_level level() const;

private:
    friend class adc_input;
    // Can be called only from adc_input class.
    adc_input_v_5v(): adc_input(ADC_LINE_5V) {}

    void _signal_report() const;
};

class adc_input_v_con: public adc_input {
public:
    bool is_connected_level() const { return read() < NOMINAL_LEVEL - 200u; }

private:
    friend class adc_input;
    // Can be called only from adc_input class.
    adc_input_v_con(uint8_t line): adc_input(line) {}

    void _signal_report() const;

    const uint16_t NOMINAL_LEVEL = _NOMINAL_LEVELS[line];
    static constexpr uint16_t _NOMINAL_LEVELS[] = {
        0,  // for v_5v, not used here.
        ADC_CON1_NOMINAL, ADC_CON2_NOMINAL
    };
};
