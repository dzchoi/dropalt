#pragma once

#include "thread.h"
#include "thread_flags.h"

#include "features.hpp"



class adc_thread {
public:
    static adc_thread& instance() {
        static adc_thread instance;
        return instance;
    }

    // Wait until v_5v stays above ADC_5V_START_LEVEL for around 10 ms. Waiting is
    // indefinite, blocking the calling thread, and may cause a watchdog reset if it waits
    // too long.
    void wait_for_stable_5v();

    uint16_t v_5v() const { return m_v_5v; }

    // void signal_usb_reset() { thread_flags_set(m_pthread, FLAG_USB_RESET); };
    // void signal_usb_suspend() { thread_flags_set(m_pthread, FLAG_USB_SUSPEND); };
    // void signal_usb_resume() { thread_flags_set(m_pthread, FLAG_USB_RESUME); };

    adc_thread(const adc_thread&) =delete;
    adc_thread& operator=(const adc_thread&) =delete;

private:
    adc_thread();

    static constexpr size_t THREAD_PRIORITY_ADC = THREAD_PRIORITY_MAIN - 1;

    // thread body
    static void* _adc_thread(void* arg);

    thread_t* m_pthread;
    char m_stack[THREAD_STACKSIZE_TINY];

    enum {
        FLAG_EVENT          = 0x0001,  // == THREAD_FLAG_EVENT from event.h
        FLAG_TIMER          = THREAD_FLAG_TIMEOUT  // (1u << 14)
    };

    uint16_t m_v_5v;  // voltage on ADC_LINE_5V
    uint16_t m_v_con1;
    uint16_t m_v_con2;
};
