#pragma once

#include <cstdint>
#include "sam_usb.h"            // sam0_common_usb_t

#include "features.hpp"
#include "usbus_hid_ext.hpp"



struct report_keyboard_t;
struct report_mouse_t;

// 0 = Boot Protocol, 1 = Report Protocol (default)
// (Only) the host (e.g. bios) can pull it down to boot protocol using
// USB_HID_REQUEST_SET_PROTOCOL. Keyboard cannot set it to 0 on its own.
// Todo: It should be volatile (and atomic) in principle for other threads to access
// safely, but it's ok because it will change only at the USB device enumeration and
// it would not change between two accesses in the same function.
extern uint8_t keyboard_protocol;

class usb_thread {
public:
    // As a singleton class we can call usb_thread::obj() to get the object. But, beware
    // to not call usb_thread::obj() from within the constructor usb_thread::usb_thread().
    // It will end up with an infinite loop.
    static usb_thread& obj() {
        static usb_thread obj;
        return obj;
    }

    void async_send_keyboard(report_keyboard_t* report);
    void async_send_system(uint16_t data);
    void async_send_consumer(uint16_t data);
    void async_send_mouse(report_mouse_t* report);

    // Synchronous operations
    int console_send(const void* buf, size_t len);
    int console_printf(const char* fmt, ...);

    void async_send_remote_wake_up();

    bool is_state_configured()
    {
        return m_usbus.state == USBUS_STATE_CONFIGURED;
    }

    bool is_state_suspended()
    {
        return m_usbus.state == USBUS_STATE_SUSPEND;
    }

    // See 38.8.1.4 Finite State Machine Status in SAM D5x/E5x Family Data Sheet
    uint8_t fsmstatus()
    {
        return ((sam0_common_usb_t*)m_usbus.dev)->config->device->FSMSTATUS.reg;
    }

    usb_thread(const usb_thread&) =delete;
    usb_thread& operator=(const usb_thread&) =delete;

private:
    usb_thread();

    thread_t* m_pthread;
    char m_stack[USBUS_STACKSIZE];

    usbus_t m_usbus;

    // Boot protocol keyboard
    usbus_hid_keyboard_t hid_boot;

    usbus_hid_raw_t hid_raw;

    // Note that MOUSE_SHARED_EP is automatically set to yes by tmk_core/common.mk if
    // KEYBOARD_SHARED_EP is yes.
    usbus_hid_mouse_t hid_mouse;

    // SharedEP keyboard and an (optional) NKRO keyboard
    usbus_hid_shared_t hid_shared;

#ifdef KEYBOARD_SHARED_EP
    usbus_hid_device_ext_t& hid_keyboard = hid_shared;
#else
    usbus_hid_device_ext_t& hid_keyboard = hid_boot;
#endif

    usbus_hid_console_t hid_console;
};
