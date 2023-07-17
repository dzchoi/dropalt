// Disabling logs will not compile keycode_to_name[] in hid_keycodes.hpp.
// #define LOCAL_LOG_LEVEL LOG_NONE

#include "log.h"
#include "mutex.h"
#include "ztimer.h"             // for ztimer_set(), ztimer_remove()

#include "adc_thread.hpp"       // for signal_usb_suspend(), signal_usb_resume()
#include "features.hpp"         // for DELAY_USB_ACCESSIBLE_AFTER_RESUMED_MS
#include "keymap_thread.hpp"    // for signal_lamp_state(), signal_usb_accessible()
#include "main_thread.hpp"      // for signal_usb_suspend(), signal_usb_resume()
#include "pmap.hpp"             // for lamp_iter, lamp_id()
#include "rgb_thread.hpp"       // for signal_usb_suspend(), signal_usb_resume()
#include "usb_thread.hpp"       // for send_remote_wake_up()
#include "usbus_hid_keyboard.hpp"



void usbus_hid_keyboard_t::help_usb_init(
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
    m_report_updated = 0;
    m_press_yet_to_submit = KC_NO;
    // However, we do not clear m_led_lamp_state as it will be initialized separately
    // from host (_hdlr_receive_data()).

    // The main_thread does not monitor the flag yet.
    // main_thread::obj().signal_usb_reset();
}

void usbus_hid_keyboard_t::on_suspend()
{
    ztimer_remove(ZTIMER_MSEC, &m_delay_usb_accessible);
    m_is_usb_accessible = false;

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

void usbus_hid_keyboard_t::_tmo_usb_accessible(void* arg)
{
    usbus_hid_keyboard_t* const hidx = static_cast<usbus_hid_keyboard_t*>(arg);
    keymap_thread::obj().signal_usb_accessible();
    // We set m_is_usb_accessible = true after signaling USB being accessible, so that
    // keymap_thread can finish handling the signal before report_press/release() works
    // on the accessible USB.
    hidx->m_is_usb_accessible = true;
}

// Low-latency key registering algorithm:
//  Note that Low-latency means that there will be no or little delay between a key event
//  occurring and sending the event to the host.
//  1. If it is the first key event (press/release) in current packet frame, we update the
//     report and submit it to the host now.
//  2. For a second key event in the current packet frame, we only update the report and
//     do not submit it. We will submit it at the start of next packet frame instead.
//  3. For any subsequent events in the current packet frame, we update the report and
//     overwrite the existing report but again without submitting it, except for the
//     following events, which shall block the caller thread (keymap_thread) to delay the
//     processing until next packet frame:
//       = any press*,
//       = any modifier release,
//       = the release whose press was updated in this frame, and
//  4. At the start of the next packet frame, i.e. when the submission for the first key
//     event is acknowledged by the host,
//       = if report has been further updated in step 2 or 3 above, we submit the updated
//         report now, which is counted as the first event in this packet frame, and
//       = then resume the blocked thread (if any) to carry out the delayed processing
//         now.
// * This condition, however, could be mitigated in 6KRO protocol.
//   Even in NKRO protocol, we could submit a modifier along with a non-modifier press
//   in the same report.
//
//  Note that `in_lock` mutex is used to wait until the next packet frame (host polling)
//  rather than to protect `in_buf` from simultaneous accesses, for which m_report_updated
//  is used instead (See submit_report() below).

bool usbus_hid_keyboard_t::report_press(uint8_t keycode)
{
    if ( !m_is_usb_accessible ) {
        LOG_DEBUG("USB_HID: press (0x%x %s) in suspend mode\n",
                keycode, keycode_to_name[keycode]);
        usb_thread::obj().send_remote_wake_up();
        return false;
    }

    if ( m_report_updated > 0 )
        // We acquire mutex lock first time when m_report_updated == 1 if it is not
        // acquired already by report_release(). Then we always acquire it again for any
        // press, which will block the caller thread (keymap_thread). Note that the first
        // time mutex lock does not block the thread but it enables the thread to be
        // blocked when the mutex lock is acquired again.
        mutex_lock(&in_lock);

    if ( update_report(keycode, true) ) {
        if ( m_report_updated++ == 0 ) {
            LOG_DEBUG("USB_HID: register press (0x%x %s)\n",
                keycode, keycode_to_name[keycode]);
            submit_report();
        }
        else {
            LOG_DEBUG("USB_HID: defer press (0x%x %s)\n",
                keycode, keycode_to_name[keycode]);
            m_press_yet_to_submit = keycode;
        }
    }

    return true;
}

bool usbus_hid_keyboard_t::report_release(uint8_t keycode)
{
    if ( !m_is_usb_accessible ) {
        LOG_DEBUG("USB_HID: release (0x%x %s) in suspend mode\n",
                keycode, keycode_to_name[keycode]);
        return false;
    }

    if ( m_report_updated > 0 )
    if ( m_report_updated == 1
      || keycode == m_press_yet_to_submit || keycode >= KC_LCTRL )
        // We acquire mutex lock first time when m_report_updated == 1 if it is not
        // acquired already by report_press(). Then we acquire it again only when a
        // modifier is released or when a key is released whose press is yet to submit.
        mutex_lock(&in_lock);

    if ( update_report(keycode, false) ) {
        if ( m_report_updated++ == 0 ) {
            LOG_DEBUG("USB_HID: register release (0x%x %s)\n",
                keycode, keycode_to_name[keycode]);
            submit_report();
        }
        else
            LOG_DEBUG("USB_HID: defer release (0x%x %s)\n",
                keycode, keycode_to_name[keycode]);
    }

    return true;
}

void usbus_hid_keyboard_t::submit_report()
{
    occupied = ep_in->maxpacketsize;
    fill_in_buf();
    usbus_event_post(usbus, &tx_ready);
}

void usbus_hid_keyboard_t::on_transfer_complete()
{
    // We are done with current packet frame.
    LOG_DEBUG("USB_HID: --------\n");

    if ( m_report_updated > 1 ) {
        LOG_DEBUG("USB_HID: register deferred events\n");
        // This needs fixing Riot's _usbus_thread() to be able to handle multiple events
        // on event queue, as this submit_report() triggers another event while handling
        // an event.
        submit_report();
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
    LOG_WARNING("USB_HID: tx_timer expired!\n");
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

bool usbus_hid_keyboard_t::help_update_bits(uint8_t& bits, uint8_t keycode, bool is_press)
{
    const uint8_t mask = uint8_t(1) << (keycode & 7);
    if ( ((bits & mask) != 0) == is_press ) {
        LOG_ERROR("USB_HID: Key (0x%x %s) is already %sed\n",
            keycode, keycode_to_name[keycode], is_press ? "press" : "releas");
        return false;
    }

    if ( is_press )
        bits |= mask;
    else
        bits &= ~mask;
    return true;
}

bool usbus_hid_keyboard_t::help_update_skro_report(
    uint8_t keys[], uint8_t keycode, bool is_press)
{
    size_t i;
    for ( i = 0 ; i < SKRO_KEYS_SIZE && keys[i] != KC_NO ; i++ )
        if ( keys[i] == keycode ) {
            if ( is_press ) {
                LOG_ERROR("USB_HID: Key (0x%x %s) is already pressed\n",
                    keycode, keycode_to_name[keycode]);
                return false;
            } else {
                while ( ++i < SKRO_KEYS_SIZE && keys[i] )
                    keys[i-1] = keys[i];
                keys[--i] = KC_NO;
                return true;
            }
        }

    if ( !is_press ) {
        LOG_ERROR("USB_HID: Key (0x%x %s) is already released\n",
            keycode, keycode_to_name[keycode]);
        return false;
    }

    if ( i == SKRO_KEYS_SIZE ) {
        LOG_WARNING("USB_HID: no room to report key press (0x%x %s)\n",
            keycode, keycode_to_name[keycode]);
        return false;
    }

    keys[i] = keycode;
    return true;
}

bool usbus_hid_keyboard_t::help_update_nkro_report(
    uint8_t bits[], uint8_t keycode, bool is_press)
{
    if ( (keycode >> 3) >= NKRO_KEYS_SIZE ) {
        LOG_WARNING("USB_HID: Key (0x%x) is out of NKRO report range\n", keycode);
        return false;
    }

    return help_update_bits(bits[keycode >> 3], keycode, is_press);
}

void usbus_hid_keyboard_t::_hdlr_receive_data(
    usbus_hid_device_t* hid, uint8_t* data, size_t len)
{
    usbus_hid_keyboard_t* const hidx = static_cast<usbus_hid_keyboard_t*>(hid);
    uint8_t lamp_state;

    if ( len == 2 ) {
        // used only for Shared EP but retained as a reference.
        const uint8_t report_id = data[0];
        if ( report_id == REPORT_ID_KEYBOARD || report_id == REPORT_ID_NKRO ) {
            lamp_state = data[1];
        }
        else
            return;
    } else
        lamp_state = data[0];

    LOG_DEBUG("USB_HID: set led_lamp_state=0x%x @%lu\n",
        lamp_state, ztimer_now(ZTIMER_MSEC));

    lamp_state ^= hidx->m_led_lamp_state;
    for ( auto slot = lamp_iter::begin() ; slot != lamp_iter::end() ; ++slot )
        // Notify only those slots whose lamp state is to be changed.
        if ( lamp_state & (uint8_t(1) << slot->lamp_id()) )
            keymap_thread::obj().signal_lamp_state(&*slot);

    // Update m_led_lamp_state with the received data.
    hidx->m_led_lamp_state ^= lamp_state;

    // Start the m_delay_usb_accessible timer now if USB is not accessible.
    if ( !hidx->m_is_usb_accessible )
        ztimer_set(ZTIMER_MSEC,
            &hidx->m_delay_usb_accessible, DELAY_USB_ACCESSIBLE_AFTER_RESUMED_MS);
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
void usbus_hid_mouse_t::usb_init(usbus_t* usbus)
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
void usbus_hid_shared_t::usb_init(usbus_t* usbus)
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
