#include "assert.h"
#include "log.h"
#include "usbus_ext.h"          // for usbus_t, usbus_init(), usbus_create(), ...
#include "thread.h"             // for thread_get()

#include "config.hpp"           // for ENABLE_NKRO, ENABLE_CDC_ACM
#include "usb_thread.hpp"
#include "usbus_hid_keyboard.hpp" // for usbus_hid_keyboard_tl<>



extern "C" void usb_cdc_acm_stdio_init(usbus_t* usbus);

thread_t* usb_thread::m_pthread = nullptr;

char usb_thread::m_thread_stack[USBUS_STACKSIZE];

usbus_t usb_thread::m_usbus;

usbus_hid_keyboard_t* usb_thread::m_hid_keyboard = nullptr;

// Any log outputs before initializing the usb_thread will be lost, as the log buffer
// (cdcacm->tsrb) is not yet set up. Once the usb_thread is running, logs will be put
// into the buffer and transmitted to the host, potentially delayed until the host
// connects.
void usb_thread::init()
{
    usbus_init(&m_usbus, usbdev_get_ctx(0));  // Or m_usbus.dev = usbdev_get_ctx(0);

    // Initialize CDC ACM first. Otherwise, CDC ACM may not work properly on Windows; if
    // the HID stack is initialized first and allocates endpoints, it may reserve the
    // endpoints needed by CDC ACM, potentially preventing it from working properly.
    if constexpr ( ENABLE_CDC_ACM )
        usb_cdc_acm_stdio_init(&m_usbus);

    static usbus_hid_keyboard_tl<ENABLE_NKRO> _hid_keyboard(&m_usbus);
    m_hid_keyboard = &_hid_keyboard;

    // Create "usbus" thread.
    usbus_create(
        m_thread_stack, sizeof(m_thread_stack), THREAD_PRIO_USB, USBUS_TNAME, &m_usbus);
    m_pthread = thread_get(m_usbus.pid);
    assert( m_pthread != nullptr );
}

void usb_thread::send_remote_wake_up()
{
    // "First, the USB must have detected a 'Suspend' state on the bus, i.e. the remote
    // wakeup request can only be sent after INTFLAG.SUSPEND has been set. The user may
    // then write a one to the Remote Wakeup bit (CTRLB.UPRSM) to send an Upstream Resume
    // to the host initiating the wakeup. This will automatically be done by the
    // controller after 5 ms of inactivity on the USB bus. When the controller sends the
    // Upstream Resume INTFLAG.WAKEUP is set and INTFLAG.SUSPEND is cleared.
    // The CTRLB.UPRSM is cleared at the end of the transmitting Upstream Resume."

    UsbDevice* const device = ((sam0_common_usb_t*)m_usbus.dev)->config->device;
    if ( is_state_suspended() && (device->CTRLB.reg & USB_DEVICE_CTRLB_UPRSM) == 0 ) {
        device->CTRLB.reg |= USB_DEVICE_CTRLB_UPRSM;
        LOG_INFO("USB_HID: send remote wakeup request\n");
    }
}
