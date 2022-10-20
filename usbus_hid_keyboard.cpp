#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "adc_thread.hpp"
#include "main_thread.hpp"
#include "usbus_hid_keyboard.hpp"
#include "usb_thread.hpp"       // for hid_keyboard.report_release()



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

bool usbus_hid_keyboard_t::help_report_press(uint8_t keycode, bool is_tapping)
{
    bool ok = true;

    auto it = m_reported_keys.begin();
    while ( it != m_reported_keys.end() && it->code != keycode )
        ++it;

    if ( it == m_reported_keys.end() ) {
        // Newly pressed
        it = m_reported_keys.emplace(it, keycode);
        DEBUG("Keyboard: register press of 0x%x\n", keycode);
        ok = report_key(keycode, true);
    }
    else {
        // Already pressed
        ++it->ref_count;
        DEBUG("Keyboard:\e[0;31m ignore press of 0x%x (ref_count=%d)\e[0m\n",
            keycode, it->ref_count);
    }

    // Todo: Instead of using a timer we can delay the reverting changes that are made
    //  within 10 ms and thus will not be reported to the host, to the next usb interrupt
    //  frame.
    if ( ok && is_tapping )
        xtimer_set(&it->tapping_timer, TAPPING_RELEASE_DELAY_US);

    return ok;
}

bool usbus_hid_keyboard_t::help_report_release(uint8_t keycode)
{
    auto it = m_reported_keys.begin();
    while ( it != m_reported_keys.end() && it->code != keycode )
        ++it;

    if ( it == m_reported_keys.end() ) {
        DEBUG("Keyboard:\e[0;31m Key (0x%x) is already released\e[0m\n", keycode);
        return false;
    }

    if ( --it->ref_count == 0 ) {
        m_reported_keys.erase(it);
        DEBUG("Keyboard: register release of 0x%x\n", keycode);
        report_key(keycode, false);
    }
    else
        DEBUG("Keyboard:\e[0;31m ignore release of 0x%x (ref_count=%d)\e[0m\n",
            keycode, it->ref_count);

    return true;
}

void usbus_hid_keyboard_t::_hdlr_report_press(event_t* event)
{
    usbus_hid_keyboard_t* const that =
        static_cast<event_ext_t<usbus_hid_keyboard_t*, uint8_t>*>(event)->arg0;
    uint8_t keycode =
        static_cast<event_ext_t<usbus_hid_keyboard_t*, uint8_t>*>(event)->arg1;
    that->help_report_press(keycode, false);
}

void usbus_hid_keyboard_t::_hdlr_report_tap(event_t* event)
{
    usbus_hid_keyboard_t* const that =
        static_cast<event_ext_t<usbus_hid_keyboard_t*, uint8_t>*>(event)->arg0;
    uint8_t keycode =
        static_cast<event_ext_t<usbus_hid_keyboard_t*, uint8_t>*>(event)->arg1;
    that->help_report_press(keycode, true);
}

void usbus_hid_keyboard_t::_hdlr_report_release(event_t* event)
{
    usbus_hid_keyboard_t* const that =
        static_cast<event_ext_t<usbus_hid_keyboard_t*, uint8_t>*>(event)->arg0;
    uint8_t keycode =
        static_cast<event_ext_t<usbus_hid_keyboard_t*, uint8_t>*>(event)->arg1;
    that->help_report_release(keycode);
}

void usbus_hid_keyboard_t::reported_key_t::_tmo_delayed_release(void* arg)
{
    reported_key_t* const that = static_cast<reported_key_t*>(arg);
    usb_thread::obj().hid_keyboard.report_release(that->code);
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

bool usbus_hid_keyboard_t::help_skro_report_key(
    uint8_t keys[], uint8_t keycode, bool pressed)
{
    size_t i;
    for ( i = 0 ; i < SKRO_KEYS_SIZE && keys[i] != KC_NO ; i++ )
        if ( keys[i] == keycode ) {
            if ( pressed ) {
                // Todo: Once m_reported_keys is proved out working fine, convert the
                //  DEBUGs with "\e[1;31m" into assert().
                DEBUG("Keyboard:\e[1;31m Key (0x%x) is already pressed\e[0m\n", keycode);
                return false;
            } else {
                changed = false;  // Disable _tmo_automatic_report() while updating.
                while ( ++i < SKRO_KEYS_SIZE && keys[i] )
                    keys[i-1] = keys[i];
                keys[--i] = KC_NO;
                changed = true;
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

    changed = false;  // Disable _tmo_automatic_report() while updating.
    keys[i] = keycode;
    changed = true;
    return true;
}

bool usbus_hid_keyboard_t::help_nkro_report_key(
    uint8_t bits[], uint8_t keycode, bool pressed)
{
    if ( (keycode >> 3) >= NKRO_KEYS_SIZE ) {
        DEBUG("Keyboard:\e[0;31m Key (0x%x) is out of NKRO report range\e[0m\n", keycode);
        return false;
    }

    const bool ok = help_report_bits(bits[keycode >> 3], keycode, pressed);
    if ( !ok )
        DEBUG("Keyboard:\e[1;31m Key (0x%x) is already %sed\e[0m\n", keycode,
            pressed ? "press" : "releas");
    return ok;
}

void usbus_hid_keyboard_tl<NKRO>::set_protocol(uint8_t protocol)
{
    usbus_hid_keyboard_t::set_protocol(protocol);
    xkro_report_key = (protocol == 0)
        ? &usbus_hid_keyboard_tl::skro_report_key
        : &usbus_hid_keyboard_tl::nkro_report_key;
}
