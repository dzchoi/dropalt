#pragma once

#include "sam_usb.h"            // for sam0_common_usb_t
#include "thread.h"             // for thread_t
#include "usbus_ext.h"          // for usbus_t

#include "usbus_hid_keyboard.hpp"



class usb_thread {
public:
    static void init();

    // Send a remote wakeup to the host if it is suspended. If the data (D+) line is
    // already disconnected (e.g. due to switchover, cable disconnect, or power-down),
    // the host will simply ignore the wakeup request. This does not affect how key
    // events are backfilled into the key event queue during suspension.
    // Note: While sending remote wakeup only when the data line is active can be
    // helpful, Linux and Windows often maintain an active connection during shutdown,
    // making it unreliable to distinguish between suspend and full shutdown.
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
