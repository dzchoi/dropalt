#pragma once

// Watchdog timeout for debugging aid; if matrix scan is not performed for this period
// long, we jump to the bootloader.
#define WDT_TIMEOUT                     (4 *1000U)      // 4 sec

// Undefine this to not blink debug LED while USB suspend.
#define LED_BLINK_TIMER_DURING_SUSPEND  (1 *1000000U)   // 1 sec

// Define this to run rgb_matrix_task() in a separate thread and make smooth updating
// of RGB LEDs. Effective only when RGB_MATRIX_ENABLE.
#define RGB_TASK_IN_SEPARATE_THREAD

// Turn off all LEDs when suspending.
// No need to call rgb_matrix_set_suspend_state() from suspend_wakeup_init_kb() and
// suspend_power_down_kb() in addtion to defining it, since they are already handled so.
#define RGB_DISABLE_WHEN_USB_SUSPENDED  true
