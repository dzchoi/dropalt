#include "usb_thread.hpp"



extern "C" void usb_cdc_acm_stdio_init(usbus_t* usbus);

usb_thread::usb_thread()
: m_usbus(usbdev_get_ctx(0))
, hid_keyboard(&m_usbus)
, hid_raw(&m_usbus)  // registered as the 2nd interface (Interface number is 1).
{
    if constexpr ( VIRTSER_ENABLE )
        // USB serial console (USB CDC-ACM device) connected on /dev/ttyACMx on Linux
        // Todo: Why does it take so long for CDC_ACM to settle down?
        usb_cdc_acm_stdio_init(&m_usbus);

    // Create "usbus" thread.
    usbus_create(m_stack, USBUS_STACKSIZE, THREAD_PRIO_USB, USBUS_TNAME, &m_usbus);
    m_pthread = thread_get(m_usbus.pid);
}

/*
#ifdef EXTRAKEY_ENABLE
static void _send_extrakey(uint8_t report_id, uint16_t data)
{
    report_extra_t report = { .report_id = report_id, .usage = data };
    hid_shared.submit((const uint8_t*)&report, sizeof(report_extra_t));
}
#endif

void usb_thread::async_send_system(uint16_t data)
{
#ifdef EXTRAKEY_ENABLE
    _send_extrakey(REPORT_ID_SYSTEM, data);
#else
    (void)data;
#endif
}

void usb_thread::async_send_consumer(uint16_t data)
{
#ifdef EXTRAKEY_ENABLE
    _send_extrakey(REPORT_ID_CONSUMER, data);
#else
    (void)data;
#endif
}

void usb_thread::async_send_mouse(report_mouse_t* report)
{
#ifdef MOUSE_ENABLE
#   ifdef MOUSE_SHARED_EP
    hid_shared.submit((const uint8_t*)report, sizeof(report_mouse_t));
#   else
    hid_mouse.submit((const uint8_t*)report, sizeof(report_mouse_t));
#   endif
#else
    (void)report;
#endif
}
*/

// Required to be called only when m_usbus.state == USBUS_STATE_SUSPEND.
void usb_thread::send_remote_wake_up(void)
{
    // "First, the USB must have detected a “Suspend” state on the bus, i.e. the remote
    // wakeup request can only be sent after INTFLAG.SUSPEND has been set.
    // The user may then write a one to the Remote Wakeup bit (CTRLB.UPRSM) to send an
    // Upstream Resume to the host initiating the wakeup. This will automatically be
    // done by the controller after 5 ms of inactivity on the USB bus.
    // When the controller sends the Upstream Resume INTFLAG.WAKEUP is set and INTFLAG.
    // SUSPEND is cleared.
    // The CTRLB.UPRSM is cleared at the end of the transmitting Upstream Resume."

    UsbDevice* const device = ((sam0_common_usb_t*)m_usbus.dev)->config->device;

    if ( (device->CTRLB.reg & USB_DEVICE_CTRLB_UPRSM) == 0 )
        device->CTRLB.reg |= USB_DEVICE_CTRLB_UPRSM;
}
