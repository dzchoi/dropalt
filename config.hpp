// Configurable user settings

#pragma once

#include <cstdint>              // for uint32_t, ...
#include "time_units.h"         // for MS_PER_SEC



// The ENABLE_* constant here will activate or deactivate the corresponding feature in
// the source code at compile-time, for example, using if constexpr (ENABLE_CDC_ACM).

// Serial over USB (CDC_ACM), enabling the stdout.
constexpr bool ENABLE_CDC_ACM = true;

// Enable Lua REPL and also enable the stdin.
constexpr bool ENABLE_LUA_REPL = true;

// ENABLE_LUA_REPL requires ENABLE_CDC_ACM.
static_assert( !ENABLE_LUA_REPL || ENABLE_CDC_ACM );

constexpr bool ENABLE_NKRO = true;

// Periodic interval for reporting keyboard state change to the host. Updates occur at
// this rate (recommended range: 1–10 ms). In Boot protocol mode, this setting is
// overridden and set to 10 ms.
constexpr uint8_t KEYBOARD_REPORT_INTERVAL_MS = 10;

// While USB is suspended, key events are stored in the event queue to be processed once
// USB reconnects. These events remain in the queue for this duration, which should
// exceed the typical switchover time (~1 second).
constexpr uint32_t SUSPENDED_KEY_EVENT_LIFETIME_MS = 4 *MS_PER_SEC;

// Wait before USB becomes accessible after resumption. A delay that's too short may
// cause buffered key events from suspend mode to be missed upon USB resume.
constexpr uint32_t DELAY_USB_ACCESSIBLE_AFTER_RESUMED_MS = 500;

// If false, power to the extra port will be cut off during USB suspend.
constexpr bool KEEP_CHARGING_EXTRA_DEVICE_DURING_SUSPEND = true;

// Extra port (v_con1/con2) measurement period.
constexpr uint32_t EXTRA_PORT_MEASURING_PERIOD_MS = 5;

// When v_5v stays at low level for this period we cut off the extra device.
constexpr uint32_t GRACE_TIME_TO_CUT_EXTRA_MS = 1 *MS_PER_SEC;  // 1 second

// Debug LED blinks every this period while USB is being connected or while USB is
// suspended. Set to 0 to disable the blinking.
constexpr uint32_t DEBUG_LED_BLINK_PERIOD_MS = 1 *MS_PER_SEC;  // 1 second

// Default retry period for connecting USB, if DEBUG_LED_BLINK_PERIOD_MS == 0.
constexpr uint32_t DEFAULT_USB_RETRY_PERIOD_MS = 1 *MS_PER_SEC;  // 1 second

// GCR changes slowly and gracefully, changing 1 GCR per this time period.
constexpr uint32_t RGB_GCR_CHANGE_PERIOD_MS = 32;

// NVM (SmartEEPROM) is delayed to write for this period.
constexpr uint32_t NVM_WRITE_DELAY_MS = 1000;

// Keyboard matrix scan rate (while operating in active scan mode)
constexpr uint32_t MATRIX_SCAN_PERIOD_US = 997;  // ~1 ms.

// A key press sustained for this duration will make a debounced press.
constexpr uint8_t DEBOUNCE_PRESS_MS = 3;  // must be >= 1.

// A key release sustained for this duration will make a debounced release.
constexpr uint8_t DEBOUNCE_RELEASE_MS = 8;  // must be >= 1.
