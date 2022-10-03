// User-facing feature configurations

#pragma once

// #define MOUSE_ENABLE
// #define EXTRAKEY_ENABLE
// #define NKRO_ENABLE

#define KEYBOARD_SHARED_EP
// #define MOUSE_SHARED_EP

// #define RAW_ENABLE
// #define CONSOLE_ENABLE
#define VIRTSER_ENABLE

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
constexpr uint32_t DEBOUNCE_TIME_US = 3 *1000u;



#ifdef KEYBOARD_SHARED_EP
// With the current usb_descriptor.c code,
// you can't share kbd without sharing mouse;
// that would be a very unexpected use case anyway
#   define MOUSE_SHARED_EP
#endif

#if defined(KEYBOARD_SHARED_EP) || defined(MOUSE_SHARED_EP) || defined(EXTRAKEY_ENABLE) || defined(NKRO_ENABLE)
#   define SHARED_EP_ENABLE
#endif



#ifdef KEYBOARD_SHARED_EP
#    define KEYBOARD_REPORT_SIZE 9
#else
#    define KEYBOARD_REPORT_SIZE 8
#endif
