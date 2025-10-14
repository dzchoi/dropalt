// #define LOCAL_LOG_LEVEL LOG_NONE

#include "irq.h"                // for irq_disable(), irq_restore()
#include "log.h"
#include "ztimer.h"             // for ztimer_set(), ztimer_remove()

#include "config.hpp"           // for USB_RESUME_SETTLE_MS, ...
#include "main_thread.hpp"      // for signal_lamp_state(), signal_thread_idle(), ...
#include "usb_thread.hpp"       // for send_remote_wake_up()
#include "usbhub_thread.hpp"    // for signal_usb_suspend(), signal_usb_resume()
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

    // Set the polling interval for the interrupt IN endpoint.
    ep_in->interval = ep_interval_ms;
    usbus_enable_endpoint(ep_in);

    // This call returns the bInterfaceNumber of the newly added interface, which is
    // also accessible via hid->iface->idx. Explicitly saving it is usually unnecessary,
    // since most operations can retrieve it directly from the hid instance they operate
    // on.
    usbus_add_interface(usbus, &iface);
}

void usbus_hid_keyboard_t::on_reset()
{
    // Restore the default full-featured protocol (1) after USB reset.
    set_protocol(1);

    // Reset the report state to prevent residual keypresses from leaking into the next
    // environment. For example, exiting BIOS via Ctrl+Alt+Del may leave keys logically
    // active. The new environment will call on_reset() to flush prior key state in this
    // case.
    // Note: However, if the old environment remains active during a switchover,
    // residual key states may persist (especially on Windows) if the old environment
    // doesn't explicitly clear them. This can be avoided through one of the followings:
    //  - Manually invoke on_reset() and submit_report() before switchover to release
    //    all pressed keys.
    //  - Ensure no physical keys are involved when performing switchover.
    //  - Call fw.switchover() through fw.execute_later(), ensuring the switchover
    //    occurs when usb_thread and matrix_thread are idle.
    clear_report();
    m_report_updated = 0;
    m_press_yet_to_submit = KC_NO;

    // main_thread::signal_usb_reset();  // Not used.
}

void usbus_hid_keyboard_t::on_suspend()
{
    ztimer_remove(ZTIMER_MSEC, &m_timer_resume_settle);
    m_is_usb_accessible = false;

    // These threads should be created before usb_thread is.
    usbhub_thread::signal_usb_suspend();
    // main_thread::signal_usb_suspend();  // Not used.
}

void usbus_hid_keyboard_t::on_resume()
{
    usbhub_thread::signal_usb_resume();
    // main_thread::signal_usb_resume();  // Not used.
}

void usbus_hid_keyboard_t::_tmo_resume_settle(void* arg)
{
    usbus_hid_keyboard_t* const hidx = static_cast<usbus_hid_keyboard_t*>(arg);
    ztimer_remove(ZTIMER_MSEC, &hidx->m_timer_clear_queue);

    LOG_DEBUG("USB_HID: USB accessible @%lu", ztimer_now(ZTIMER_MSEC));
    hidx->m_is_usb_accessible = true;

    if ( hidx->m_key_event_queue.not_empty() ) {
        // Begin processing key events that were queued while USB was inaccessible.
        usbus_event_post(hidx->usbus, &hidx->m_event_transfer_complete);
    }
}

// Low-latency key reporting algorithm:
// "Low-latency" here refers to minimal delay between a key event and its transmission
// to the host. The basic idea of this algorithm is ensuring:
//   - No two key presses are reported in the same packet frame*.
//   - A press and its corresponding release are not reported within the same frame.
//
// 1. If the key event (press/release) is the first in the current packet frame:
//    - Update the report.
//    - Submit it to the host immediately.
// 2. If it's the second event in the current packet frame:
//    - Update the report, overwriting the previous contents.
//    - Do not submit; the updated report will be sent at the start of the next frame.
// 3. For any additional events:
//    - If the event is
//        = A key press,
//        = A release whose corresponding press was reported in step 2 or 3,
//        = A modifier release, if ANY press was reported in step 2 or 3.
//     then delay it (along with all subsequent events) until the current packet frame
//     is delivered to the host.
//    - Otherwise, update the report without submitting and repeat step 3.
// 4. At the start of the next packet frame (after the host acknowledges the submission
//    made in step 1):
//    - If the report was updated during step 2 or 3, submit it now, and this acts as
//      step 1 for the new frame.
//    - Resume processing any delayed events from step 3.
//
// * This condition, however, can be relaxed in 6KRO mode. Even in NKRO mode, a
//   non-modifier press can be reported alongside a modifier press in the same frame.

// This method is not thread-safe, but is always invoked either from usb_thread or via
// report_event() from client thread — both contexts ensure thread safety. To preserve
// event ordering when sending to the host, this method must not be called again within
// the same packet frame if returning false. Both report_event() and
// on_transfer_complete() respect this constraint.
bool usbus_hid_keyboard_t::try_report_event(uint8_t keycode, bool is_press)
{
    if ( m_report_updated > 1 )
        if ( is_press || keycode == m_press_yet_to_submit
          || ( m_press_yet_to_submit != KC_NO && keycode >= KC_LCTRL ) )
            return false;

    if ( update_report(keycode, is_press) ) {
        if ( m_report_updated++ == 0 ) {
            LOG_DEBUG("USB_HID: register %s (0x%x %s)",
                press_or_release(is_press), keycode, keycode_to_name[keycode]);
            submit_report();
        }
        else {
            LOG_DEBUG("USB_HID: defer %s (0x%x %s)",
                press_or_release(is_press), keycode, keycode_to_name[keycode]);
            if ( is_press )
                m_press_yet_to_submit = keycode;
        }
    }

    return true;
}

// This method is supposed to execute from client thread (main_thread).
void usbus_hid_keyboard_t::report_event(uint8_t keycode, bool is_press)
{
    unsigned state = irq_disable();  // Disable preemption by usb_thread or interrupt.

    // While USB is suspended, key events are still added to the event queue so they can
    // take effect once USB resumes. These events remain in the queue only for
    // USB_SUSPEND_EVENT_TIMEOUT_MS.
    if ( unlikely(!m_is_usb_accessible) ) {
        LOG_DEBUG("USB_HID: key %s in suspend mode", press_or_release(is_press));
        if ( is_press )
            usb_thread::send_remote_wake_up();
        m_key_event_queue.push({keycode, is_press});

        // Start m_timer_clear_queue, or extend its duration if the timer is already
        // running.
        ztimer_set(ZTIMER_MSEC, &m_timer_clear_queue, USB_SUSPEND_EVENT_TIMEOUT_MS);
    }

    else if ( m_key_event_queue.not_empty() || !try_report_event(keycode, is_press) )
        // m_is_usb_accessible is true and we allow push() to block.
        m_key_event_queue.push({keycode, is_press}, true);

    irq_restore(state);
}

void usbus_hid_keyboard_t::submit_report()
{
    occupied = ep_in->maxpacketsize;
    fill_in_buf();
    usbus_event_post(usbus, &tx_ready);
}

// This method is invoked from an event handler (e.g. m_event_transfer_complete or
// usbdev_ep_esr()) within usb_thread context. However, it is safe to call from
// interrupt context as well, since submit_report() and try_report_event() only post
// events and perform lightweight memory operations.
void usbus_hid_keyboard_t::on_transfer_complete(bool was_successful)
{
    // If USB suspends during an active transfer, do nothing here — on_reset() is
    // responsible for resetting the report state data.
    if ( unlikely(!m_is_usb_accessible) )
        return;

    if ( was_successful && m_report_updated > 1 ) {
        LOG_DEBUG("USB_HID: register deferred events");
        // Calling submit_report() here will post a new event while another event is
        // being handled, which requires Riot's _usbus_thread() to be updated to handle
        // multiple queued events per invocation.
        submit_report();
        m_report_updated = 1;
    }
    else {
        m_report_updated = 0;
        main_thread::signal_thread_idle();
    }
    m_press_yet_to_submit = KC_NO;

    // Process remaining events from the key event queue, pushing as many as fit into
    // the packet frame. If an event cannot be pushed, exit early and resume processing
    // at the next frame.
    usb_key_events::key_event_t event;
    while ( m_key_event_queue.peek(event)
      && try_report_event(event.keycode, event.is_press) )
        m_key_event_queue.pop();
}



// Keyboard HID report formats:
//
// * Standard 8-byte report (6KRO):
//   Retains the state of 8 modifier keys and up to 6 concurrent key presses.
//   byte |0       |1       |2       |3       |4       |5       |6       |7
//   -----+--------+--------+--------+--------+--------+--------+--------+--------
//   desc |mods    |reserved|keys[0] |keys[1] |keys[2] |keys[3] |keys[4] |keys[5]
//
// * Extended 16-byte report (NKRO):
//   Supports up to 120 keys + 8 modifiers using a bitfield.
//   byte |0       |1       |2       |3       |4       |5       |6        ... |15
//   -----+--------+--------+--------+--------+--------+--------+--------     +--------
//   desc |mods    |bits[0] |bits[1] |bits[2] |bits[3] |bits[4] |bits[5]  ... |bit[14]
//
// * Modifier key bit assignments (shared for both formats):
//    bit |0       |1       |2       |3       |4       |5       |6       |7
//   -----+--------+--------+--------+--------+--------+--------+--------+--------
//   desc |Lcontrol|Lshift  |Lalt    |Lgui    |Rcontrol|Rshift  |Ralt    |Rgui

bool usbus_hid_keyboard_t::help_update_bits(uint8_t& bits, uint8_t keycode, bool is_press)
{
    const uint8_t mask = uint8_t(1) << (keycode & 7);
    if ( ((bits & mask) != 0) == is_press ) {
        LOG_ERROR("USB_HID: Key (0x%x %s) is already %sed",
            keycode, keycode_to_name[keycode], is_press ? "press" : "releas");
        return false;
    }

    is_press ? bits |= mask : bits &= ~mask;
    return true;
}

bool usbus_hid_keyboard_t::help_update_skro_report(
    uint8_t keys[], uint8_t keycode, bool is_press)
{
    size_t i;
    for ( i = 0 ; i < SKRO_KEYS_SIZE && keys[i] != KC_NO ; i++ )
        if ( keys[i] == keycode ) {
            if ( is_press ) {
                LOG_ERROR("USB_HID: Key (0x%x %s) is already pressed",
                    keycode, keycode_to_name[keycode]);
                return false;
            }
            else {
                while ( ++i < SKRO_KEYS_SIZE && keys[i] != KC_NO )
                    keys[i-1] = keys[i];
                keys[--i] = KC_NO;
                return true;
            }
        }

    if ( !is_press ) {
        LOG_ERROR("USB_HID: Key (0x%x %s) is already released",
            keycode, keycode_to_name[keycode]);
        return false;
    }

    if ( i == SKRO_KEYS_SIZE ) {
        LOG_WARNING("USB_HID: no room to report key press (0x%x %s)",
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
        LOG_WARNING("USB_HID: Key (0x%x) is out of NKRO report range", keycode);
        return false;
    }

    return help_update_bits(bits[keycode >> 3], keycode, is_press);
}

// Note that lamp state is updated only by the host in response to KC_CAPSLOCK,
// typically via a SET_REPORT request.
void usbus_hid_keyboard_t::_hdlr_receive_data(
    usbus_hid_device_t* hid, uint8_t* data, size_t len)
{
    usbus_hid_keyboard_t* const hidx = static_cast<usbus_hid_keyboard_t*>(hid);
    uint8_t lamp_state;

    if ( len == 2 ) {
        // used only for Shared EP but retained as a reference.
        const uint8_t report_id = data[0];
        if ( !(report_id == REPORT_ID_KEYBOARD || report_id == REPORT_ID_NKRO) )
            return;
        lamp_state = data[1];
    } else
        lamp_state = data[0];

    LOG_DEBUG("USB_HID: set led_lamp_state=0x%x @%lu",
        lamp_state, ztimer_now(ZTIMER_MSEC));

    main_thread::signal_lamp_state(lamp_state);

    // Start m_timer_resume_settle only after receiving the first host-initiated data
    // (e.g. LED Set_Report), not immediately upon usbus_control invoking on_resume().
    if ( !hidx->m_is_usb_accessible )
        ztimer_set(ZTIMER_MSEC, &hidx->m_timer_resume_settle, USB_RESUME_SETTLE_MS);
}

void usbus_hid_keyboard_tl<NKRO>::set_protocol(uint8_t protocol)
{
    usbus_hid_keyboard_t::set_protocol(protocol);
    xkro_report_key = (protocol == 0)
        ? &usbus_hid_keyboard_tl::skro_report_key
        : &usbus_hid_keyboard_tl::nkro_report_key;
}



/*
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
