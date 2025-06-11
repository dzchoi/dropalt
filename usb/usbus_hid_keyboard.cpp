// Disabling logs will not compile keycode_to_name[] in hid_keycodes.hpp.
// #define LOCAL_LOG_LEVEL LOG_NONE

#include "assert.h"             // for assert()
#include "irq.h"                // for irq_disable(), irq_enable(), irq_restore()
#include "log.h"
#include "mutex.h"              // for mutex_lock(), mutex_unlock(), ...
#include "ztimer.h"             // for ztimer_set(), ztimer_remove()

#include "config.hpp"           // for DELAY_USB_ACCESSIBLE_AFTER_RESUMED_MS, ...
#include "main_thread.hpp"      // for signal_usb_resume(), signal_lamp_state(), ...
// #include "lamp.hpp"             // for lamp_iter, lamp_id()
// #include "rgb_thread.hpp"       // for signal_usb_suspend(), signal_usb_resume()
#include "usb_thread.hpp"       // for send_remote_wake_up()
#include "usbhub_thread.hpp"    // for signal_usb_suspend(), signal_usb_resume()
#include "usbus_hid_keyboard.hpp"



void key_event_queue_t::push(key_event_t event, bool wait_if_full)
{
    LOG_DEBUG("USB_HID: queue %s (0x%x %s)\n",
        press_or_release(event.is_press), event.keycode, keycode_to_name[event.keycode]);

    if ( mutex_trylock(&m_not_full) == 0 && wait_if_full ) {
        // push() is designed to be called in thread context with interrupts disabled to
        // maintain thread safety. When waiting for a mutex here, however, interrupts and
        // other threads must be allowed to run to prevent deadlock. While a condition
        // variable could serve the same purpose, using interrupts minimizes the footprint.
        unsigned state = irq_enable();
        mutex_lock(&m_not_full);  // wait until it is unlocked.
        irq_restore(state);
    }

    m_events[m_end++ & (QUEUE_SIZE - 1)] = event;
    if ( not_full() )
        // If queue is full the mutex remains locked.
        mutex_unlock(&m_not_full);
    else
        // This prevents the queue from holding more events than QUEUE_SIZE.
        m_begin = m_end - QUEUE_SIZE;
}

bool key_event_queue_t::pop()
{
    if ( not_empty() ) {
        m_begin++;
        mutex_unlock(&m_not_full);
        return true;
    }
    return false;
}

void key_event_queue_t::clear()
{
    LOG_DEBUG("USB_HID: clear key event queue\n");
    m_begin = m_end;
    mutex_unlock(&m_not_full);
}

bool key_event_queue_t::peek(key_event_t& event) const
{
    if ( not_empty() ) {
        event = m_events[m_begin & (QUEUE_SIZE - 1)];
        return true;
    }
    return false;
}



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

    // Interrupt endpoint polling rate in ms
    ep_in->interval = ep_interval_ms;
    usbus_enable_endpoint(ep_in);

    // This will return the bInterfaceNumber of the added interface, which is also
    // accessible via hid->iface->idx. However, saving it is likely unnecessary, as all
    // operations can directly retrieve it from the hid that they oprate on.
    usbus_add_interface(usbus, &iface);
}

void usbus_hid_keyboard_t::on_reset()
{
    set_protocol(1);

    // The m_report_updated and m_press_yet_to_submit will be reset later when USB is
    // accessible. m_led_lamp_state will also be reset separately when indicated from host
    // (_hdlr_receive_data()).

    // The main_thread does not monitor the flag yet.
    main_thread::signal_usb_reset();
}

void usbus_hid_keyboard_t::on_suspend()
{
    ztimer_remove(ZTIMER_MSEC, &m_delay_usb_accessible);
    m_is_usb_accessible = false;

    // These threads should be created before usb_thread is.
    // if constexpr ( RGB_DISABLE_WHEN_USB_SUSPENDS )
    //     rgb_thread::obj().signal_usb_suspend();
    usbhub_thread::signal_usb_suspend();
    main_thread::signal_usb_suspend();
}

void usbus_hid_keyboard_t::on_resume()
{
    // if constexpr ( RGB_DISABLE_WHEN_USB_SUSPENDS )
    //     rgb_thread::obj().signal_usb_resume();
    usbhub_thread::signal_usb_resume();
    main_thread::signal_usb_resume();
}

void usbus_hid_keyboard_t::_tmo_usb_accessible(void* arg)
{
    usbus_hid_keyboard_t* const hidx = static_cast<usbus_hid_keyboard_t*>(arg);
    ztimer_remove(ZTIMER_MSEC, &hidx->m_timer_clear_queue);

    LOG_DEBUG("USB_HID: USB accessible\n");
    hidx->m_is_usb_accessible = true;
    usbus_event_post(hidx->usbus, &hidx->m_event_reset_transfer);
}

// Low-latency key registering algorithm:
//  Note that Low-latency here means that there will be no or little delay between a key
//  event occurring and sending the event to the host. The basic idea of this algorithm
//  is ensuring that
//   - no two presses are reported in the same packet frame*, and
//   - a press and its release are not reported in the same packet frame.
//
//  1. If it is the first key event (press/release) in current packet frame, we update the
//     report and submit it to the host immediately.
//  2. For a second key event in the current packet frame, we update the report,
//     overwriting the existing report, but do not submit it. We will submit it at the
//     start of next packet frame instead.
//  3. For any subsequent event, we also update the report and do not submit it, except
//     for the following events, which shall be delayed to process until the current
//     packet frame ends and any further events shall be delayed too.
//      = a press,
//      = the release whose press was updated at step 2 or 3, and
//      = a modifier release if there was any prior press at step 2 or 3.
//  4. At the start of the next packet frame, i.e. when the host acknowledges the
//     submission of the first report at step 1,
//      = if report has been further updated at step 2 or 3 above, we submit the updated
//        report now, which will be counted as the first report in the new packet frame,
//      = then resume processing any delayed events.
//
// * This condition, however, could be mitigated in 6KRO protocol. Even in NKRO protocol,
//   we could report a non-modifier press along with a modifier press in the same frame.

// This method is not thread-safe, but it is always invoked either from usb_thread or from
// a client thread via report_event(), both of which ensure thread-safety. To maintain
// the correct event order when sending to the host, this method must not be called again
// within the same packet frame after returning false. report_event() and
// on_transfer_complete() adhere to this constraint.
bool usbus_hid_keyboard_t::try_report_event(uint8_t keycode, bool is_press)
{
    if ( m_report_updated > 1 )
        if ( is_press || keycode == m_press_yet_to_submit ||
             ( m_press_yet_to_submit != KC_NO && keycode >= KC_LCTRL ) )
            return false;

    if ( update_report(keycode, is_press) ) {
        if ( m_report_updated++ == 0 ) {
            LOG_DEBUG("USB_HID: register %s (0x%x %s)\n",
                press_or_release(is_press), keycode, keycode_to_name[keycode]);
            submit_report();
        }
        else {
            LOG_DEBUG("USB_HID: defer %s (0x%x %s)\n",
                press_or_release(is_press), keycode, keycode_to_name[keycode]);
            if ( is_press )
                m_press_yet_to_submit = keycode;
        }
    }

    return true;
}

// This method is supposed to execute from a client thread (main_thread).
void usbus_hid_keyboard_t::report_event(uint8_t keycode, bool is_press)
{
    unsigned state = irq_disable();  // Disable preemption by usb_thread or interrupt.

    // When USB is in suspended mode, key events are still added to the event queue to
    // ensure they take effect once USB reconnects. These events remain in the queue only
    // for SUSPENDED_KEY_EVENT_LIFETIME_MS.
    if ( !m_is_usb_accessible ) {
        LOG_DEBUG("USB_HID: key event in suspend mode\n");
        if ( is_press )
            usb_thread::send_remote_wake_up();
        m_key_event_queue.push({keycode, is_press});

        // Start m_timer_clear_queue, or extend m_timer_clear_queue if it is already
        // running.
        ztimer_set(ZTIMER_MSEC, &m_timer_clear_queue, SUSPENDED_KEY_EVENT_LIFETIME_MS);
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

// This method is called from an event handler (via m_event_reset_transfer or
// usbdev_ep_esr()) in usb_thread context. However, it would be OK to be called from
// an interrupt context, since submit_report() and try_report_event() below only throw an
// event along with some memory operations performed.
void usbus_hid_keyboard_t::on_transfer_complete(bool was_successful)
{
    assert( m_is_usb_accessible );

    if ( was_successful && m_report_updated > 1 ) {
        LOG_DEBUG("USB_HID: register deferred events\n");
        // This needs fixing Riot's _usbus_thread() to be able to handle multiple events
        // on event queue, as this submit_report() can triggers another event while
        // handling an event.
        submit_report();
        m_report_updated = 1;
    }
    else {
        occupied = 0;
        m_report_updated = 0;
    }
    m_press_yet_to_submit = KC_NO;

    // Process remaining key events in the queue, pushing as many as possible to the new
    // packet frame. When we encounter an event that cannot be pushed we quit, as we will
    // be invoked again at the next packet frame.
    key_event_queue_t::key_event_t event;
    while ( m_key_event_queue.peek(event)
      && try_report_event(event.keycode, event.is_press) )
        m_key_event_queue.pop();
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
            }
            else {
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
        if ( !(report_id == REPORT_ID_KEYBOARD || report_id == REPORT_ID_NKRO) )
            return;
        lamp_state = data[1];
    } else
        lamp_state = data[0];

    LOG_DEBUG("USB_HID: set led_lamp_state=0x%x @%lu\n",
        lamp_state, ztimer_now(ZTIMER_MSEC));

    lamp_state ^= hidx->m_led_lamp_state;
    // Update m_led_lamp_state with the received data.
    hidx->m_led_lamp_state ^= lamp_state;

    // for ( auto it = lamp_iter::begin() ; it != lamp_iter::end() ; ++it )
    //     if ( lamp_state & (uint8_t(1) << it->lamp_id()) )
    //         main_thread::signal_lamp_state(&*it);

    // Begin the m_delay_usb_accessible timer upon receiving the first data
    // (led_lamp_state) from the host.
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
