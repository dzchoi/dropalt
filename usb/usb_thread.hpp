#pragma once

#include "sam_usb.h"            // for sam0_common_usb_t
#include "thread.h"             // for thread_t
#include "usbus_ext.h"          // for usbus_t

#include "usbus_hid_keyboard.hpp"



class usb_thread {
public:
    static void init();

    // Attempting a remote wake-up on a suspended but disconnected host may fail due to
    // host OS restrictions. However, it remains allowed as a precaution (see
    // usbus_hid_keyboard_t::report_press()). Additionally, remote wake-up can be sent to
    // a non-existent host (e.g., an unconnected port), causing the USB driver (SAM D5x
    // HW module) to immediately suspend and initiate an automatic switchover (see
    // usbport::help_process_usb_suspend()).
    static void send_remote_wake_up();

    static bool is_state_configured() { return m_usbus.state == USBUS_STATE_CONFIGURED; }

    static bool is_state_suspended() { return m_usbus.state == USBUS_STATE_SUSPEND; }

    // See 38.8.1.4 Finite State Machine Status in SAM D5x datasheet
    static uint8_t fsmstatus() {
        return ((sam0_common_usb_t*)m_usbus.dev)->config->device->FSMSTATUS.reg;
    }

    static void send_press(uint8_t keycode) {
        m_hid_keyboard->report_press(keycode);
    }

    static void send_release(uint8_t keycode) {
        m_hid_keyboard->report_release(keycode);
    }

private:
    constexpr usb_thread() =delete;  // Ensure a static class

    static thread_t* m_pthread;

    static char m_thread_stack[];

    static usbus_t m_usbus;

    static usbus_hid_keyboard_t* m_hid_keyboard;
};
