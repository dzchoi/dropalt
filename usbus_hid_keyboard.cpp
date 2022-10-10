#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "adc_thread.hpp"
#include "main_thread.hpp"
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
    main_thread::obj().signal_usb_suspend();
}

void usbus_hid_keyboard_t::on_resume()
{
    adc_thread::obj().signal_usbhub_active();
    main_thread::obj().signal_usb_resume();
}

void usbus_hid_keyboard_t::_hdlr_receive_data(
    usbus_hid_device_t* hid, uint8_t* data, size_t len)
{
    usbus_hid_keyboard_t* const hidx = static_cast<usbus_hid_keyboard_t*>(hid);

    if ( len == 2 ) {
        // for Shared EP, not used but retained as a reference.
        const uint8_t report_id = data[0];
        if ( report_id == REPORT_ID_KEYBOARD || report_id == REPORT_ID_NKRO ) {
            hidx->m_keyboard_led_state = data[1];
        }
    } else
        hidx->m_keyboard_led_state = data[0];

    DEBUG("Keyboard: set keyboard_led_state=0x%x\n", hidx->m_keyboard_led_state);
}



// * Keyboard report is 8-byte array retains state of 8 modifiers and 6 keys.
//
// byte |0       |1       |2       |3       |4       |5       |6       |7
// -----+--------+--------+--------+--------+--------+--------+--------+--------
// desc |mods    |reserved|keys[0] |keys[1] |keys[2] |keys[3] |keys[4] |keys[5]
//
// It is exended to 16 bytes to retain 120 keys + 8 mods for NKRO mode.
//
// byte |0       |1       |2       |3       |4       |5       |6       |7        ... |15
// -----+--------+--------+--------+--------+--------+--------+--------+--------     +--------
// desc |mods    |bits[0] |bits[1] |bits[2] |bits[3] |bits[4] |bits[5] |bits[6]  ... |bit[14]

// * mods retains state of 8 modifiers.
//
//  bit |0       |1       |2       |3       |4       |5       |6       |7
// -----+--------+--------+--------+--------+--------+--------+--------+--------
// desc |Lcontrol|Lshift  |Lalt    |Lgui    |Rcontrol|Rshift  |Ralt    |Rgui

bool usbus_hid_keyboard_t::help_skro_press(uint8_t keys[], uint8_t key)
{
    size_t i;
    for ( i = 0 ; i < SKRO_KEYS_SIZE && keys[i] != KC_NO ; i++ )
        if ( keys[i] == key ) {
            DEBUG("Keyboard: Key (0x%x) is already pressed\n", key);
            return false;
        }

    if ( i == SKRO_KEYS_SIZE ) {
        DEBUG("Keyboard: no room to report key (0x%x) press\n", key);
        return false;
    }

    changed = false;  // Disable _tmo_automatic_report() while updating.
    keys[i] = key;
    changed = true;
    return true;
}

bool usbus_hid_keyboard_t::help_skro_release(uint8_t keys[], uint8_t key)
{
    for ( size_t i = 0 ; i < SKRO_KEYS_SIZE && keys[i] != KC_NO ; i++ )
        if ( keys[i] == key ) {
            changed = false;  // Disable _tmo_automatic_report() while updating.
            while ( ++i < SKRO_KEYS_SIZE && keys[i] )
                keys[i-1] = keys[i];
            keys[--i] = KC_NO;
            changed = true;
            return true;
        }

    DEBUG("Keyboard: Key (0x%x) is already released\n", key);
    return false;
}

bool usbus_hid_keyboard_t::help_nkro_press(uint8_t bits[], uint8_t key)
{
    if ( (key >> 3) >= NKRO_KEYS_SIZE ) {
        DEBUG("Keyboard: Key (0x%x) is out of NKRO report range\n", key);
        return false;
    }

    const bool ok = help_bits_press(bits[key >> 3], key);
    if ( !ok )
        DEBUG("Keyboard: Key (0x%x) is already pressed\n", key);
    return ok;
}

bool usbus_hid_keyboard_t::help_nkro_release(uint8_t bits[], uint8_t key)
{
    if ( (key >> 3) >= NKRO_KEYS_SIZE ) {
        DEBUG("Keyboard: Key (0x%x) is out of NKRO report range\n", key);
        return false;
    }

    const bool ok = help_bits_release(bits[key >> 3], key);
    if ( !ok )
        DEBUG("Keyboard: Key (0x%x) is already released\n", key);
    return ok;
}

void usbus_hid_keyboard_tl<NKRO>::set_protocol(uint8_t protocol)
{
    usbus_hid_keyboard_t::set_protocol(protocol);

    if ( protocol == 0 ) {
        xkro_press = &usbus_hid_keyboard_tl::skro_press;
        xkro_release = &usbus_hid_keyboard_tl::skro_release;
    } else {
        xkro_press = &usbus_hid_keyboard_tl::nkro_press;
        xkro_release = &usbus_hid_keyboard_tl::nkro_release;
    }
}
