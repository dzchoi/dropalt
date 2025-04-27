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
