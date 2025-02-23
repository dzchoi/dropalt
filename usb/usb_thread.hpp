#pragma once

#include "thread.h"             // for thread_t
#include "usbus_ext.h"          // for usbus_t



// This usb class is a singleton. Use usb::thread() to access the instance.
class usb {
public:
    static void init();

    static usb& thread() { return m_instance; }

private:
    static usb m_instance;

    thread_t* m_pthread;

    usbus_t m_usbus;

    usb() =default;  // for creating the static m_instance.
};
