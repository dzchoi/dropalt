#pragma once

#include "sam_usb.h"            // for sam0_common_usb_t

#include "features.hpp"         // for *_ENABLE
#include "usbus_hid_keyboard.hpp"
#include "usbus_hid_raw.hpp"



class usb_thread {
public:
    // As a singleton class we can call usb_thread::obj() to get the object. But, beware
    // to not call usb_thread::obj() from within the constructor usb_thread::usb_thread().
    // It will end up with an infinite loop.
    static usb_thread& obj() {
        static usb_thread obj;
        return obj;
    }

    void send_remote_wake_up();

    bool is_state_configured() { return m_usbus.state == USBUS_STATE_CONFIGURED; }

    bool is_state_suspended() { return m_usbus.state == USBUS_STATE_SUSPEND; }

    // See 38.8.1.4 Finite State Machine Status in SAM D5x/E5x Family Data Sheet
    uint8_t fsmstatus() {
        return ((sam0_common_usb_t*)m_usbus.dev)->config->device->FSMSTATUS.reg;
    }

    usb_thread(const usb_thread&) =delete;
    usb_thread& operator=(const usb_thread&) =delete;

private:
    usb_thread();

    thread_t* m_pthread;
    char m_stack[USBUS_STACKSIZE];

    struct usbus_same_t: usbus_t {
        usbus_same_t(usbdev_t* usbdev) { usbus_init(this, usbdev); }
    } m_usbus;

public:
    usbus_hid_keyboard_tl<NKRO_ENABLE> hid_keyboard;

    usbus_hid_raw_tl<RAW_ENABLE> hid_raw;
};
