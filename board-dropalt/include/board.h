/*
 * Copyright (C) 2022 dzchoi <19231860+dzchoi@users.noreply.github.com>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     board-dropalt
 * @{
 *
 * @file
 * @brief       Board specific definitions for Drop Alt
 *
 * @author      dzchoi <19231860+dzchoi@users.noreply.github.com>
 */

#pragma once

#include "periph/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name    xtimer configuration
 * @{
 */
// We use the default configuration as:
// #define XTIMER_DEV          TIMER_DEV(0)
// #define XTIMER_CHAN         (0)
// #define XTIMER_WIDTH        (32)
// #define XTIMER_HZ           (1000000ul)
/** @} */

/**
 * @name   LED pin definitions and handlers
 * @{
 */
#define LED0_PIN            GPIO_PIN(PA, 27)
#define LED0_ON             gpio_set(LED0_PIN)
#define LED0_OFF            gpio_clear(LED0_PIN)
#define LED0_TOGGLE         gpio_toggle(LED0_PIN)
/** @} */

#if 0
/**
 * @name SW0 (Button) pin definitions
 * @{
 */
#define BTN0_PORT           PORT->Group[PA]
#define BTN0_PIN            GPIO_PIN(PA, 15)
#define BTN0_MODE           GPIO_IN_PU
/** @} */
#endif

/**
 * @name    USB configuration
 * @note    periph_usbdev needs to know these custom CONFIG_USB_* constants.
 * @{
 */
// USB keyboard descriptor parameters
#define VENDOR_ID           0x04D8
#define PRODUCT_ID          0xEED3
#define DEVICE_VER          0x0101

#define MANUFACTURER        "Massdrop Inc."
#define PRODUCT             "ALT Keyboard"
#define SAME_SERIAL         "Same as in Massdrop Hub"

// USB HUB descriptor parameters
#define HUB_VENDOR_ID       0x04D8
#define HUB_PRODUCT_ID      0xEEC5
#define HUB_DEVICE_VER      0x0101

#define HUB_MANUFACTURER    "Massdrop Inc."
#define HUB_PRODUCT         "Massdrop Hub"
#define HUB_NO_SERIAL       "Unavailable"   // to be used when not factory-programmed

// USB keyboard descriptor parameters for RIOT (USB 2.0 spec table 9-8)
#define CONFIG_USB_SPEC_BCDVERSION_2_0      1       // bcdUSB == 0x0200
#define CONFIG_USBUS_EP0_SIZE               64      // bMaxPacketSize0
#define CONFIG_USB_VID                 VENDOR_ID    // idVendor
#define CONFIG_USB_PID                 PRODUCT_ID   // idProduct
#define CONFIG_USB_PRODUCT_BCDVERSION  DEVICE_VER   // bcdDevice (not used by RIOT)
#define CONFIG_USB_MANUF_STR           MANUFACTURER // iManufacturer
#define CONFIG_USB_PRODUCT_STR         PRODUCT      // iProduct
#define CONFIG_USB_SERIAL_STR          SAME_SERIAL  // iSerialNumber

// Factory-programmed serial number could be read from bootloader as following, but it
// is in utf-16 format, which cannot be given to RIOT's usbus module at compile-time.
// extern uint32_t _sfixed;
// #define CONFIG_USB_SERIAL_STR (const uint16_t*)*(uint32_t*)((uint32_t)&_sfixed - 4)

// USB configuration descriptor (USB 2.0 spec table 9-10)
#define CONFIG_USB_MAX_POWER            0   // bMaxPower (USB HUB will report 500 mA.)
// #define CONFIG_USB_CONFIGURATION_STR "USB config" // iConfiguration
#define CONFIG_USB_REM_WAKEUP           1  // bmAttributes

// Buffer size for STDIN and STDOUT data to and from USB for USBUS_CDC_ACM_STDIO module
#define CONFIG_USBUS_CDC_ACM_STDIO_BUF_SIZE (4096)

// Used in keyboard and mouse USB interfaces. This also determines the keyboard_task()
// running period, and the rgb_matrix_task() too if RGB_TASK_IN_SEPARATE_THREAD is not
// defined.
#define USB_POLLING_INTERVAL_MS         10

#define KEYBOARD_EPSIZE                 8
#define MOUSE_EPSIZE                    8
#define SHARED_EPSIZE                   32
#define CONSOLE_EPSIZE                  32
#define RAW_EPSIZE                      32

#define CONSOLE_PRINTBUF_SIZE           512
/** @} */

// Watchdog timeout for debugging aid; if matrix scan is not performed for this period
// long, we jump to the bootloader.
#define WDT_TIMEOUT                     (4 *1000U)      // 4 sec

//extern uint32_t _srom;
extern uint32_t _sfixed;
extern uint32_t _lrom;
extern uint32_t _erom;

#define BOOTLOADER_SERIAL_MAX_SIZE 20  // DO NOT MODIFY!

/**
 * @name    ADC threshold parameters
 * @{
 */
#define ADC_5V_START_LEVEL              2365

// Automatic power rollback and recovery
// 5V high level (After low power detect, point at which LEDs are allowed to use more power)
#define ADC_5V_HIGH                     2540

// 5V low level (LED power rolled back to stay above this limit)
#define ADC_5V_LOW                      2480

// 5V catastrophic level (Host USB port potential to shut down)
#define ADC_5V_CAT                      2200

// median value of adcs between plugged CON1 (= ~1040) and unplugged CON2 (= ~1140)
#define USB_EXTRA_ADC_THRESHOLD         1090
/** @} */



/**
 * @brief   System reset using Application Interrupt and Reset Control Register (AIRCR).
 * @note    Restarts the firmware without jumping to bootloader.
 */
// inline void system_reset(void) { __NVIC_SystemReset(); }
#define system_reset()      __NVIC_SystemReset()


/**
 * @brief   Initialize board specific hardware, including clock, LEDs and std-IO
 */
void board_init(void);

#ifdef __cplusplus
}
#endif

/** @} */
