// User-facing feature configurations

#pragma once

#include <stdint.h>             // for uint8_t
#include "time_units.h"         // for US_PER_MS



// These *_ENABLE constants will enable/disable the corresponding feature and also
// add/remove the source code that implements the feature.
// (e.g. if constexpr (VIRTSER_ENABLE)).

constexpr bool NKRO_ENABLE = true;
// #define MOUSE_ENABLE
// #define EXTRAKEY_ENABLE

// Interval for keyboard reporting to the host. Keyboard changes are reported to the host
// periodically at this rate (1-10 ms recommended). When keyboard is working in Boot
// protocol this value is ignored and fixed to 10 ms.
constexpr uint8_t KEYBOARD_REPORT_INTERVAL_MS = 10;

// Hid Raw device
constexpr bool RAW_ENABLE = false;
constexpr uint8_t RAW_REPORT_INTERVAL_MS = 1;
// #define CONSOLE_PRINTBUF_SIZE           512

// Serial over USB (CDC_ACM)
constexpr bool VIRTSER_ENABLE = true;

// Note that the data packet size for every endpoint is defined as *_REPORT_SIZE in
// usb_descriptor.hpp.

// Undefine this to not blink debug LED while USB suspend.
#define LED_BLINK_TIMER_DURING_SUSPEND  (1 *1000000U)   // 1 sec

// Define this to run rgb_matrix_task() in a separate thread and make smooth updating
// of RGB LEDs. Effective only when RGB_MATRIX_ENABLE.
#define RGB_TASK_IN_SEPARATE_THREAD

// Turn off all LEDs when suspending.
// No need to call rgb_matrix_set_suspend_state() from suspend_wakeup_init_kb() and
// suspend_power_down_kb() in addtion to defining it, since they are already handled so.
#define RGB_DISABLE_WHEN_USB_SUSPENDED  true

// Keyboard matrix scan rate (while operating in timer-based scan mode)
constexpr uint32_t MATRIX_SCAN_PERIOD_US = 887u;

// Keys are registered only after this time period of no bounces.
// If too short, it may double a register, but too long period may miss.
constexpr uint32_t DEBOUNCE_TIME_US = 3 *US_PER_MS;

// The delay time for release after hid_keyboard.report_tap().
constexpr uint32_t TAPPING_RELEASE_DELAY_US = 40 *US_PER_MS;

// TAPPING_TERM_US is the maximum time from press to release to be counted as a tap.
constexpr uint32_t TAPPING_TERM_US = 200 *US_PER_MS;
