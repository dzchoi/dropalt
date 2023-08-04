// User-facing feature configurations

#pragma once

#include <stdint.h>             // for uint8_t
#include "time_units.h"         // for MS_PER_SEC

#include "color.hpp"            // for hsv_t



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

// Only key events up to this age will be buffered during USB suspend mode. It should be
// greater than the typical time for switchover (= ~1 second).
constexpr uint32_t MAX_AGE_OF_KEY_EVENTS_BUFFERED_DURING_SUSPEND_MS = 4 *MS_PER_SEC;

// Delay for USB being accessible after USB is resumed. If too short, we can miss to send
// some key events that were buffered during USB suspend mode, when USB is resumed.
constexpr uint32_t DELAY_USB_ACCESSIBLE_AFTER_RESUMED_MS = 500;

// Enable keyboard emulator to automate key events.
constexpr bool ENABLE_EMULATOR = true;

// Magic word to start emulator.
constexpr uint16_t EMULATOR_MAGIC_WORD = ('[' | '}' << 8);

// Hid Raw device
constexpr bool RAW_ENABLE = false;
constexpr uint8_t RAW_REPORT_INTERVAL_MS = 1;
// #define CONSOLE_PRINTBUF_SIZE           512

// Serial over USB (CDC_ACM)
constexpr bool VIRTSER_ENABLE = true;

// Note that the data packet size for every endpoint is defined as *_REPORT_SIZE in
// usb_descriptor.hpp.

// If false, the power will be cut off to the extra port while USB suspends.
constexpr bool KEEP_CHARGING_EXTRA_DEVICE_DURING_SUSPEND = true;

// Extra port (v_con1/con2) is measured using this period.
constexpr uint32_t EXTRA_PORT_MEASURING_PERIOD_MS = 5;

// When v_5v stays at low level for this period we cut off the extra device.
constexpr uint32_t GRACE_TIME_TO_CUT_EXTRA_MS = 1 *MS_PER_SEC;  // 1 second

// Keyboard matrix scan rate (while operating in timer-based scan mode)
constexpr uint32_t MATRIX_SCAN_PERIOD_US = 997;  // very close to 1 ms.

// If press is not detected in first scan for this duration we go back to sleep and
// perform interrupt-based scan.
constexpr uint32_t FIRST_SCAN_DURATION_MS = 1 *MS_PER_SEC;  // 1 second

// 7 consecutive releases will make a debounced release.
constexpr int8_t DEBOUNCE_PATTERN_FOR_RELEASE = 0b10000000;

// 2 consecutive presses will make a debounced press.
constexpr int8_t DEBOUNCE_MASK_FOR_PRESS      = 0b10000011;
constexpr int8_t DEBOUNCE_PATTERN_FOR_PRESS   = 0b00000011;

// TAPPING_TERM_MS is the maximum time from press to release to be counted as a tap.
constexpr uint32_t TAPPING_TERM_MS = 200;

// Todo: Undefine this to not blink debug LED while USB suspends.
constexpr uint32_t DEBUG_LED_BLINK_PERIOD_MS = 1 *MS_PER_SEC;  // 1 second

// `false` will also disable keyboard indicator lamps.
constexpr bool RGB_LED_ENABLE = true;

// Rate for updating RGB leds to show effects. 17 ms corresponds to ~60 fps.
constexpr uint32_t RGB_UPDATE_PERIOD_MS = 17;

// Turn off all LEDs while suspending.
constexpr bool RGB_DISABLE_WHEN_USB_SUSPENDS = true;

// Max Global Current Control Register (GCR) value (0-255) will limit the brightness of
// all RGB leds to reduce the power consumption.
constexpr uint8_t RGB_LED_GCR_MAX = 255;  // or 165;

// GCR changes slowly and gracefully, changing 1 GCR per this time period.
constexpr uint32_t RGB_GCR_CHANGE_PERIOD_MS = 32;

// Max number of tracers for Finger-Trace Effect.
constexpr unsigned EFFECT_FINGER_TRACE_MAX_TRACERS = 16;

// Max number of waves for Ripple Effect.
constexpr unsigned EFFECT_RIPPLE_MAX_WAVES = 8;

// Indicator lamp lights will have this solid color by default.
// constexpr hsv_t RGB_INDICATOR_COLOR = hsv_t{ 0, 0, 255 };  // white
constexpr hsv_t RGB_INDICATOR_COLOR = hsv_t{ 256, 255, 255 };  // yellow

// The identifier of the firmware that manages NVM.
constexpr uint32_t NVM_MAGIC_NUMBER = 'd' + ('r' << 8) + ('o' << 16) + ('p' << 24);

// NVM (SmartEEPROM) is delayed to write for this period.
constexpr uint32_t NVM_WRITE_DELAY_MS = 4000;
