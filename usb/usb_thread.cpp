#include "assert.h"
#include "usbus_ext.h"          // for usbus_t, usbus_init(), usbus_create(), ...
#include "thread.h"             // for thread_get()

#include "features.hpp"         // for ENABLE_CDC_ACM
#include "usb_thread.hpp"



extern "C" void usb_cdc_acm_stdio_init(usbus_t* usbus);

usb usb::m_instance;

static char _thread_stack[USBUS_STACKSIZE];

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
