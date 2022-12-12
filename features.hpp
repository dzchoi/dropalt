// User-facing feature configurations

#pragma once

#include <stdint.h>             // for uint8_t
#include "time_units.h"         // for MS_PER_SEC



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

// When we power cycle keyboard (through reset or downloading a firmware) with two hosts
// connected we try the port 1 to connect to the host, if this flag is true. Otherwise,
// we try the port 2 first.
constexpr bool POWER_UP_CHECK_PORT1_FIRST = true;

// If false, the power will be cut off to the extra port while USB suspends.
constexpr bool KEEP_CHARGING_EXTRA_DEVICE_DURING_SUSPEND = true;

// Keyboard matrix scan rate (while operating in timer-based scan mode)
constexpr uint32_t MATRIX_SCAN_PERIOD_MS = 1;  // or less than 1 ms (e.g. 887 us)?

// Keys are registered only after this time period of no bounces.
// If too short, it may double a register, but too long period may miss.
constexpr uint32_t DEBOUNCE_TIME_MS = 4;

// TAPPING_TERM_MS is the maximum time from press to release to be counted as a tap.
constexpr uint32_t TAPPING_TERM_MS = 200;

// Todo: Undefine this to not blink debug LED while USB suspends.
constexpr uint32_t DEBUG_LED_BLINK_PERIOD_MS = 1 *MS_PER_SEC;  // 1 second

constexpr bool RGB_LED_ENABLE = true;

// Rate for updating RGB leds to show effects. 17 ms corresponds to ~60 fps.
constexpr uint32_t RGB_UPDATE_PERIOD_MS = 17;

// Turn off all LEDs while suspending.
constexpr bool RGB_DISABLE_WHEN_USB_SUSPENDS = true;

// Max Global Current Control Register (GCR) value (0-255) will limit the brightness of
// all RGB leds to reduce the power consumption.
constexpr uint8_t RGB_LED_GCR_MAX = 255;  // or 165;

// GCR changes slowly and gracefully, changing 1 GCR per this time period.
constexpr uint32_t RGB_GCR_CHANGE_PERIOD_MS = 16;

// Max number of tracers for Finger-Trace Effect.
constexpr unsigned EFFECT_FINGER_TRACE_MAX_TRACERS = 16;

// Max number of waves for Ripple Effect.
constexpr unsigned EFFECT_RIPPLE_MAX_WAVES = 8;
