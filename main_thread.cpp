#define LOCAL_LOG_LEVEL LOG_INFO

#include "log.h"
#include "periph/wdt.h"         // for wdt_kick()
#include "stdio_base.h"         // for ssize_t, stdio_read()
#include "thread_flags.h"       // for thread_flags_t, thread_flags_set(), ...
#include "ztimer.h"             // for ztimer_set()

#include "adc_input.hpp"        // for v_5v, v_con1/2
#include "adc_thread.hpp"       // for adc_thread::obj()
#include "effects.hpp"          // for default effect
#include "features.hpp"         // for ENABLE_EMULATOR, RGB_LED_ENABLE, ...
#include "keymap_thread.hpp"    // for keymap_thread::obj(), signal_key_event(), ...
#include "main_thread.hpp"
#include "matrix_thread.hpp"    // for matrix_thread::obj()
#include "persistent.hpp"       // for persistent::obj(), init(), get_log(), ...
#include "rgb_thread.hpp"       // for rgb_thread::obj(), set_effect()
#include "usb_thread.hpp"       // for usb_thread::obj()



extern "C" void stdio_stop_read(void);

// main() is called from the thread entry function of main thread. See kernel_init().
int main()
{
    main_thread::obj().init();
    main_thread::obj().main();
    return 0;
}

void main_thread::signal_usb_reset()
{
    thread_flags_set(m_pthread, FLAG_USB_RESET);
    if constexpr ( ENABLE_EMULATOR ) {
        stdio_stop_read();
    }
}

void main_thread::signal_usb_suspend()
{
    thread_flags_set(m_pthread, FLAG_USB_SUSPEND);
    if constexpr ( ENABLE_EMULATOR ) {
        stdio_stop_read();
    }
}

void main_thread::signal_usb_resume()
{
    thread_flags_set(m_pthread, FLAG_USB_RESUME);
    if constexpr ( ENABLE_EMULATOR ) {
        stdio_stop_read();
    }
}

void main_thread::_tmo_keep_alive(void* arg)
{
    main_thread* const that = static_cast<main_thread*>(arg);
    thread_flags_set(that->m_pthread, FLAG_TIMEOUT);
    if constexpr ( ENABLE_EMULATOR ) {
        stdio_stop_read();
    }
}

void main_thread::init()
{
    // Setup Seeprom.
    persistent::init();

    // Create all threads in the order of dependency.
    (void)rgb_thread::obj();
    (void)adc_thread::obj();
    (void)usb_thread::obj();
    (void)keymap_thread::obj();
    (void)matrix_thread::obj();

    if constexpr ( RGB_LED_ENABLE ) {
        // static fixed_color effect = persistent::obj().led_color;
        // static glimmer effect { persistent::obj().led_color, 5 *MS_PER_SEC };
        // static finger_trace effect { persistent::obj().led_color, 5 *MS_PER_SEC };
        static ripple effect { persistent::obj().led_color, 250, 50 };
        rgb_thread::obj().set_effect(effect);
    }

    m_show_previous_logs = persistent::has_log();
}

void main_thread::main()
{
    thread_flags_t flags;
    while ( true ) {
        wdt_kick();
        // Set up wake-up alarm before going to sleep.
        ztimer_set(ZTIMER_MSEC, &m_keep_alive, KEEP_ALIVE_PERIOD_MS);

        // Zzz
        if constexpr ( ENABLE_EMULATOR ) {
            execute_emulator();
            flags = thread_flags_clear(
                FLAG_USB_RESET
                | FLAG_USB_RESUME
                | FLAG_TIMEOUT );
        }
        else {
            flags = thread_flags_wait_any(
                FLAG_USB_RESET
                | FLAG_USB_RESUME
                | FLAG_TIMEOUT );
        }

        if ( m_show_previous_logs && (flags & FLAG_USB_RESUME) ) {
            // Use LOG_INFO instead of LOG_ERROR to not add duplicate logs.
            LOG_INFO(persistent::get_log());
            m_show_previous_logs = false;
        }

        if ( flags & FLAG_TIMEOUT ) {
            // char buffer[32];
            // sprintf(buffer, "%lu\n", info);
            // usb_thread::obj().hid_raw.print(buffer);

            LOG_DEBUG(
                "v_5v=%d v_con1=%d v_con2=%d %s-mode fsmstatus=0x%x\n",
                adc_input::v_5v.read(),
                adc_input::v_con1.read(),
                adc_input::v_con2.read(),
                usb_thread::obj().hid_keyboard.get_protocol() ? "nkro" : "6kro",
                usb_thread::obj().fsmstatus() );
        }
    }
}

// The whole processing of key events from one command should be finished within
// KEEP_ALIVE_PERIOD_MS. Otherwise the execessive events will be discarded.
void main_thread::execute_emulator()
{
    struct { uint16_t magic; uint8_t command; uint8_t len; } header;
    static_assert( sizeof(header) == 4 );
    const ssize_t len = stdio_read(&header, sizeof(header));
    // Note that Linux CDC ACM driver has a problem of ECHOing any chars received between
    // opening the TTY and turning ECHO off: https://lore.kernel.org/linux-serial/d6d376ceb45b5a72c2a053721eabeddfa11cc1a5.camel@infinera.com/
    // This will cause some early LOG_DEBUG texts to be read back from stdio_read() when
    // USB is being connected. However, the ECHO triggers stdio_read() per character, thus
    // len == 1, and those characters will be filtered out by the following condition.
    if ( !( len == sizeof(header) &&
            header.magic == EMULATOR_MAGIC_WORD && header.command == 0x0f ) )
        return;  // Go back to sleep.

    struct { uint8_t index; uint8_t is_press; } event;
    static_assert( sizeof(event) == 2 );
    for ( int n = header.len ; n > 0 ; n -= sizeof(event)) {
        const ssize_t len = stdio_read(&event, sizeof(event));
        if ( len != sizeof(event) )
            return;
        (void)keymap_thread::obj().signal_key_event(event.index, event.is_press);
    }
}
