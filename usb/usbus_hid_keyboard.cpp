#include "mutex.h"

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "adc_thread.hpp"       // for signal_usb_suspend() and signal_usb_resume()
#include "main_thread.hpp"      // for signal_usb_suspend() and signal_usb_resume()
#include "rgb_thread.hpp"       // for signal_usb_suspend() and signal_usb_resume()
#include "usb_thread.hpp"       // for send_remote_wake_up()
#include "usbus_hid_keyboard.hpp"



void usbus_hid_keyboard_t::help_init(
    usbus_t* usbus, size_t epsize, uint8_t ep_interval_ms)
{
    iface._class = USB_CLASS_HID;
    iface.subclass = USB_HID_SUBCLASS_BOOT;  // We also support Boot protocol!
    iface.protocol = USB_HID_PROTOCOL_KEYBOARD;
    iface.descr_gen = &hid_descr;
    iface.handler = &handler_ctrl;

    // Register the events that the hid will listen to.
    usbus_handler_set_flag(&handler_ctrl,
        USBUS_HANDLER_FLAG_RESET
        | USBUS_HANDLER_FLAG_SUSPEND
        | USBUS_HANDLER_FLAG_RESUME);

    // IN endpoint to send data to host
    ep_in = usbus_add_endpoint(usbus, &iface,
                               USB_EP_TYPE_INTERRUPT,
                               USB_EP_DIR_IN,
                               epsize);

    // interrupt endpoint polling rate in ms
    ep_in->interval = ep_interval_ms;
    usbus_enable_endpoint(ep_in);

    // It will return the bInterfaceNumber of the added interface, which can also
    // be read using hid->iface->idx, but we will unlikely need it since all operations
    // will provide the hid that it oprates on.
    usbus_add_interface(usbus, &iface);
}

void usbus_hid_keyboard_t::on_reset()
{
    set_protocol(1);
    // eeconfig_update_keymap(keymap_config.raw);  // Todo: ???

    // The main_thread does not monitor the flag yet.
    // main_thread::obj().signal_usb_reset();
}

void usbus_hid_keyboard_t::on_suspend()
{
    // These threads should be created before usb_thread is.
    if constexpr ( RGB_DISABLE_WHEN_USB_SUSPENDS )
        rgb_thread::obj().signal_usb_suspend();
    adc_thread::obj().signal_usb_suspend();
    main_thread::obj().signal_usb_suspend();
}

void usbus_hid_keyboard_t::on_resume()
{
    if constexpr ( RGB_DISABLE_WHEN_USB_SUSPENDS )
        rgb_thread::obj().signal_usb_resume();
    adc_thread::obj().signal_usb_resume();
    main_thread::obj().signal_usb_resume();
}

// Low-latency key register algorithm:
//  - If it is the first key press/release in the current frame, update the report and
//    immediately submit it to the host.
//  - For any subsequent presses/releases in the current frame, update the report but do
//    not submit it to the host, except the following subsequent events which shall delay
//    the updating until in the next frame:
//     = any 2nd press in this frame [1],
//     = any modifier release, and
//     = the release whose press was updated in this frame.
//  - At the start of next frame, i.e. when the submission (for the first key press/
//    release) is acknowledged by the host,
//     = if report has been updated further since the last submission submit it all now,
//     = and resume the updating if it has been delayed.
//  - Note that in_lock mutex is used to wait until the next frame (host polling) as
//    opposed to protecting in_buf from simultaneous accesses, for which m_report_updated
//    is used instead (See submit() below).
//
// [1] This condition, however, could be mitigated in 6KRO protocol.
//     Even in NKRO protocol, we could submit a modifier along with a normal press in the
//     same report.

void usbus_hid_keyboard_t::report_press(uint8_t keycode)
{
    mutex_lock(&in_lock);
    if ( report_key(keycode, true) ) {
        if ( usbus_is_active() ) {
            if ( ++m_report_updated > 1 ) {
                m_press_yet_to_submit = keycode;
                DEBUG("Keyboard: defer press (0x%x)\n", keycode);
                return;
            }

            // In the case m_report_updated == 1,
            if ( keycode >= KC_A && keycode <= KC_Z )
                DEBUG("Keyboard: register press (0x%x '%c')\n",
                    keycode, 'a' - KC_A + keycode);
            else
                DEBUG("Keyboard: register press (0x%x)\n", keycode);
            submit();
        }
        else {
            // If USB is not active we still keep the report up to date but do not submit
            // to the host.
            DEBUG("Keyboard: send remote wakeup request on press (0x%x)\n", keycode);
            usb_thread::obj().send_remote_wake_up();
        }
    }
    mutex_unlock(&in_lock);
}

void usbus_hid_keyboard_t::report_release(uint8_t keycode)
{
    if ( m_press_yet_to_submit != KC_NO
      && ( keycode == m_press_yet_to_submit || keycode >= KC_LCTRL ) ) {
        // Block until the deferred press is completed.
        mutex_lock(&in_lock);
        mutex_unlock(&in_lock);
    }

    if ( report_key(keycode, false) && usbus_is_active() ) {
        if ( ++m_report_updated == 1 ) {
            DEBUG("Keyboard: register release (0x%x)\n", keycode);
            submit();
        }
        else
            DEBUG("Keyboard: defer release (0x%x)\n", keycode);
    }
}

void usbus_hid_keyboard_t::submit()
{
    occupied = ep_in->maxpacketsize;
    fill_in_buf();
    usbus_event_post(usbus, &tx_ready);
}

void usbus_hid_keyboard_t::on_transfer_complete()
{
    DEBUG("Keyboard: --------\n");
    if ( m_report_updated > 1 ) {
        DEBUG("Keyboard: register deferred events\n");
        // This needs fixing Riot's _usbus_thread() to handle multiple events on event
        // queue, as submit() triggers an event while handling another event.
        submit();
        m_report_updated = 1;
    }
    else {  // i.e. if m_report_updated == 1,
        m_report_updated = 0;
        occupied = 0;
    }

    m_press_yet_to_submit = KC_NO;
    mutex_unlock(&in_lock);
}

void usbus_hid_keyboard_t::isr_on_transfer_timeout()
{
    DEBUG("USB_HID:\e[0;31m tx_timer expired!\e[0m\n");
    m_report_updated = 0;
    m_press_yet_to_submit = KC_NO;
    occupied = 0;
    mutex_unlock(&in_lock);
}



// * Keyboard report is 8-byte array which retains states of 8 modifiers and 6 keys.
// byte |0       |1       |2       |3       |4       |5       |6       |7
// -----+--------+--------+--------+--------+--------+--------+--------+--------
// desc |mods    |reserved|keys[0] |keys[1] |keys[2] |keys[3] |keys[4] |keys[5]
//
// * It is exended to 16 bytes to retain 120 keys + 8 mods for NKRO mode.
// byte |0       |1       |2       |3       |4       |5       |6        ... |15
// -----+--------+--------+--------+--------+--------+--------+--------     +--------
// desc |mods    |bits[0] |bits[1] |bits[2] |bits[3] |bits[4] |bits[5]  ... |bit[14]
//
// * mods retain state of 8 modifiers.
//  bit |0       |1       |2       |3       |4       |5       |6       |7
// -----+--------+--------+--------+--------+--------+--------+--------+--------
// desc |Lcontrol|Lshift  |Lalt    |Lgui    |Rcontrol|Rshift  |Ralt    |Rgui

bool usbus_hid_keyboard_t::help_report_bits(uint8_t& bits, uint8_t keycode, bool pressed)
{
    const uint8_t mask = uint8_t(1) << (keycode & 7);
    if ( ((bits & mask) != 0) == pressed ) {
        DEBUG("Keyboard:\e[1;31m Key (0x%x) is already %sed\e[0m\n", keycode,
            pressed ? "press" : "releas");
        return false;
    }

    if ( pressed )
        bits |= mask;
    else
        bits &= ~mask;
    return true;
}

bool usbus_hid_keyboard_t::help_skro_report_key(
    uint8_t keys[], uint8_t keycode, bool pressed)
{
    size_t i;
    for ( i = 0 ; i < SKRO_KEYS_SIZE && keys[i] != KC_NO ; i++ )
        if ( keys[i] == keycode ) {
            if ( pressed ) {
                DEBUG("Keyboard:\e[1;31m Key (0x%x) is already pressed\e[0m\n", keycode);
                return false;
            } else {
                while ( ++i < SKRO_KEYS_SIZE && keys[i] )
                    keys[i-1] = keys[i];
                keys[--i] = KC_NO;
                return true;
            }
        }

    if ( !pressed ) {
        DEBUG("Keyboard:\e[1;31m Key (0x%x) is already released\e[0m\n", keycode);
        return false;
    }

    if ( i == SKRO_KEYS_SIZE ) {
        DEBUG("Keyboard:\e[0;31m no room to report key press (0x%x)\e[0m\n", keycode);
        return false;
    }

    keys[i] = keycode;
    return true;
}

bool usbus_hid_keyboard_t::help_nkro_report_key(
    uint8_t bits[], uint8_t keycode, bool pressed)
{
    if ( (keycode >> 3) >= NKRO_KEYS_SIZE ) {
        DEBUG("Keyboard:\e[0;31m Key (0x%x) is out of NKRO report range\e[0m\n", keycode);
        return false;
    }

    return help_report_bits(bits[keycode >> 3], keycode, pressed);
}

void usbus_hid_keyboard_t::_hdlr_receive_data(
    usbus_hid_device_t* hid, uint8_t* data, size_t len)
{
    usbus_hid_keyboard_t* const hidx = static_cast<usbus_hid_keyboard_t*>(hid);

    if ( len == 2 ) {
        // used only for Shared EP but retained as a reference.
        const uint8_t report_id = data[0];
        if ( report_id == REPORT_ID_KEYBOARD || report_id == REPORT_ID_NKRO ) {
            hidx->m_keyboard_led_state = data[1];
        }
    } else
        hidx->m_keyboard_led_state = data[0];

    DEBUG("Keyboard: set keyboard_led_state=0x%x\n", hidx->m_keyboard_led_state);
}

void usbus_hid_keyboard_tl<NKRO>::set_protocol(uint8_t protocol)
{
    usbus_hid_keyboard_t::set_protocol(protocol);
    xkro_report_key = (protocol == 0)
        ? &usbus_hid_keyboard_tl::skro_report_key
        : &usbus_hid_keyboard_tl::nkro_report_key;
}



/*
#if defined(MOUSE_ENABLE) && !defined(MOUSE_SHARED_EP)
void usbus_hid_mouse_t::init(usbus_t* usbus)
{
    iface._class = USB_CLASS_HID;
    iface.subclass = USB_HID_SUBCLASS_BOOT;
    iface.protocol = USB_HID_PROTOCOL_MOUSE;
    iface.descr_gen = &hid_descr;
    iface.handler = &handler_ctrl;

    // Register the events that the hid will listen to.
    usbus_handler_set_flag(&handler_ctrl,
        USBUS_HANDLER_FLAG_RESET
        | USBUS_HANDLER_FLAG_SUSPEND
        | USBUS_HANDLER_FLAG_RESUME);

    // IN endpoint to send data to host
    ep_in = usbus_add_endpoint(usbus, &iface,
                               USB_EP_TYPE_INTERRUPT,
                               USB_EP_DIR_IN,
                               MOUSE_EPSIZE);
    // interrupt endpoint polling rate in ms
    ep_in->interval = USB_POLLING_INTERVAL_MS;
    usbus_enable_endpoint(ep_in);

    usbus_add_interface(usbus, &iface);
}
#endif

#ifdef SHARED_EP_ENABLE
void usbus_hid_shared_t::init(usbus_t* usbus)
{
    iface._class = USB_CLASS_HID;
#ifdef KEYBOARD_SHARED_EP
    iface.subclass = USB_HID_SUBCLASS_BOOT;
    iface.protocol = USB_HID_PROTOCOL_KEYBOARD;
#else
    // Configure generic HID device interface, choosing NONE for subclass and protocol
    // in order to represent a generic I/O device
    iface.subclass = USB_HID_SUBCLASS_NONE;
    iface.protocol = USB_HID_PROTOCOL_NONE;
#endif
    iface.descr_gen = &hid_descr;
    iface.handler = &handler_ctrl;

#ifdef KEYBOARD_SHARED_EP
    // Register the events that the hid will listen to.
    usbus_handler_set_flag(&handler_ctrl,
        USBUS_HANDLER_FLAG_RESET
        | USBUS_HANDLER_FLAG_SUSPEND
        | USBUS_HANDLER_FLAG_RESUME);
#endif

    // IN endpoint to send data to host
    ep_in = usbus_add_endpoint(usbus, &iface,
                               USB_EP_TYPE_INTERRUPT,
                               USB_EP_DIR_IN,
                               SHARED_EPSIZE);
    // interrupt endpoint polling rate in ms
    ep_in->interval = USB_POLLING_INTERVAL_MS;
    usbus_enable_endpoint(ep_in);

    usbus_add_interface(usbus, &iface);
}
#endif
*/
