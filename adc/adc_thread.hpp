#pragma once

#include "event.h"
#include "thread.h"



class adc_thread {
public:
    static adc_thread& obj() {
        static adc_thread obj;
        return obj;
    }

    void signal_usb_suspend() { thread_flags_set(m_pthread, FLAG_USB_SUSPEND); }

    // Note that signal_usb_resume() (i.e. usb active) could be called by an interrupt
    // when detecting GPIO_RISING on the USB2422_ACTIVE pin (= GPIO_PIN(PA, 18)) instead.
    // However, Riot's gpio_init_int() seems assigning the same _exti (i.e. EXTINTx) to
    // the gpio pin as with row_pins[2] (= GPIO_PIN(PA, 2)), causing one of them not
    // working. The signal_usb_resume() is instead triggered by usb_thread (on
    // USBUS_EVENT_USB_RESUME). See usbus_hid_keyboard_t::on_resume().
    void signal_usb_resume() { thread_flags_set(m_pthread, FLAG_USB_RESUME); }

    void signal_usbport_switchover() {
        thread_flags_set(m_pthread, FLAG_USBPORT_SWITCHOVER);
    }

    // Signal a (generic) event to adc_thread.
    void signal_event(event_t* event) { event_post(&m_queue, event); }

    // Signal that 5v report is ready.
    void signal_report_5v() { thread_flags_set(m_pthread, FLAG_REPORT_5V); }

    // Signal that extra-port report is ready.
    void signal_report_extra() { thread_flags_set(m_pthread, FLAG_REPORT_EXTRA); }

    void signal_extra_enable_manually() {
        thread_flags_set(m_pthread, FLAG_EXTRA_MANUAL);
    }

    void signal_extra_back_to_automatic() {
        thread_flags_set(m_pthread, FLAG_EXTRA_AUTOMATIC);
    }

    adc_thread(const adc_thread&) =delete;
    void operator=(const adc_thread&) =delete;

private:
    thread_t* m_pthread;

    // THREAD_STACKSIZE_TINY cannot be used with printf().
    char m_stack[THREAD_STACKSIZE_SMALL];

    event_queue_t m_queue;  // event queue

    adc_thread();  // Can be called only by obj().

    // thread body
    static void* _adc_thread(void* arg);

    enum : uint16_t {
        FLAG_EVENT              = 0x0001,  // == THREAD_FLAG_EVENT from event.h
        FLAG_USB_SUSPEND        = 0x0002,
        FLAG_USB_RESUME         = 0x0004,
        FLAG_USBPORT_SWITCHOVER = 0x0008,
        FLAG_REPORT_5V          = 0x0010,
        FLAG_REPORT_EXTRA       = 0x0020,
        FLAG_EXTRA_MANUAL       = 0x0040,
        FLAG_EXTRA_AUTOMATIC    = 0x0080,
        FLAG_TIMEOUT            = THREAD_FLAG_TIMEOUT  // (1u << 14)
    };
};
