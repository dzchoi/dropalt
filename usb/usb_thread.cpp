#include "assert.h"
#include "usbus_ext.h"          // for usbus_t, usbus_init(), usbus_create(), ...
#include "thread.h"             // for thread_get()

#include "features.hpp"         // for ENABLE_CDC_ACM
#include "usb_thread.hpp"



extern "C" void usb_cdc_acm_stdio_init(usbus_t* usbus);

usb usb::m_instance;

static char _thread_stack[USBUS_STACKSIZE];

// Any log outputs before initializing the usb thread will be lost because the log buffer
// (cdcacm->tsrb) is not yet set up. Once the usb thread is running, logs will be put into
// the buffer and sent to the host, possibly later when the host is connected.
// Note: Some serial terminals on the PC might miss a few initial logs though. To capture
// all logs, you can use `while true; do cat /dev/ttyACMx 2>/dev/null; done`.
void usb::init()
{
    usbus_t& usbus = m_instance.m_usbus;
    usbus_init(&usbus, usbdev_get_ctx(0));

    if constexpr ( ENABLE_CDC_ACM )
        usb_cdc_acm_stdio_init(&usbus);

    // Create "usbus" thread.
    usbus_create(_thread_stack, USBUS_STACKSIZE, THREAD_PRIO_USB, USBUS_TNAME, &usbus);
    m_instance.m_pthread = thread_get(usbus.pid);
    assert( m_instance.m_pthread != nullptr );
}
