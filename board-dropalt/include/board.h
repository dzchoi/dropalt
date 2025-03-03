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

#include "compiler_hints.h"     // for NORETURN, UNREACHABLE()
#include "cpu_conf.h"
#include "periph/gpio.h"
#include "riotboot/magic.h"     // for RIOTBOOT_MAGIC_ADDR, RIOTBOOT_MAGIC_NUMBER



#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name    ztimer configuration
 * @{
 */
#define CONFIG_ZTIMER_USEC_TYPE     ZTIMER_TYPE_PERIPH_TIMER
#define CONFIG_ZTIMER_USEC_DEV      TIMER_DEV(0)
// timer_set() may underflow for values smaller than 9, set 10 as margin
#define CONFIG_ZTIMER_USEC_MIN      (10)

#define CONFIG_ZTIMER_USEC_FREQ     (1000000ul)  // 1 MHz
#define CONFIG_ZTIMER_USEC_WIDTH    (32)

// Generated by running riot/tests/ztimer_overhead.
#define CONFIG_ZTIMER_USEC_ADJUST_SET       5
#define CONFIG_ZTIMER_USEC_ADJUST_SLEEP     5
/** @} */


/**
 * @name    Debug LED pin definitions
 * @{
 */
#define LED0_PIN            GPIO_PIN(PA, 27)
#define LED0_ON             gpio_set(LED0_PIN)
#define LED0_OFF            gpio_clear(LED0_PIN)
#define LED0_TOGGLE         gpio_toggle(LED0_PIN)
/** @} */


#if 0
/**
 * @name    SW0 (Button) pin definitions
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
#ifndef DEVICE_VER
    #error DEVICE_VER not defined
#endif

#ifndef HUB_DEVICE_VER
    #error HUB_DEVICE_VER not defined
#endif

#ifdef RIOTBOOT
    // DFU-slot descriptor parameters
    // #define VENDOR_ID           0x1209
    // #define PRODUCT_ID          0x7D02
    #define VENDOR_ID           0x04D8
    #define PRODUCT_ID          0xEED3
    #define MANUFACTURER        "Massdrop"
    #define PRODUCT             "Drop ALT (bootloader)"
    #define SAME_SERIAL         "same as in Drop Hub"
#else
    // USB keyboard descriptor parameters
    #define VENDOR_ID           0x04D8
    #define PRODUCT_ID          0xEED3
    #define MANUFACTURER        "Massdrop"
    #define PRODUCT             "Drop ALT"
    #define SAME_SERIAL         "same as in Drop Hub"
#endif

// USB HUB descriptor parameters
#define HUB_VENDOR_ID       0x04D8
#define HUB_PRODUCT_ID      0xEEC5
#define HUB_MANUFACTURER    "Massdrop"
#define HUB_PRODUCT         "Drop Hub"
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

// USB configuration descriptor (USB 2.0 spec table 9-10)
#define CONFIG_USB_MAX_POWER            300  // bMaxPower (300 mA.)
// #define CONFIG_USB_CONFIGURATION_STR "USB config" // iConfiguration
#define CONFIG_USB_REM_WAKEUP           1  // bmAttributes

// Buffer size for STDIN and STDOUT data to and from USB for USBUS_CDC_ACM_STDIO module
#define CONFIG_USBUS_CDC_ACM_STDIO_BUF_SIZE (4096)
/** @} */


/**
 * @name    ADC threshold parameters
 * @{
 */
#define ADC_LINE(x) (x)
static const uint8_t ADC_LINE_5V   = ADC_LINE(0);
static const uint8_t ADC_LINE_CON1 = ADC_LINE(1);
static const uint8_t ADC_LINE_CON2 = ADC_LINE(2);

static const uint16_t ADC_5V_START_LEVEL = 2365;

// Automatic power rollback and recovery
// 5V high level (After low power detect, point at which LEDs are allowed to use more power)
static const uint16_t ADC_5V_HIGH = 2540;

// 5V low level (LED power rolled back to stay above this limit)
static const uint16_t ADC_5V_LOW = 2480;

// 5V panic level (Host USB port potential to shut down)
static const uint16_t ADC_5V_PANIC = 2200;

// Nominal level of the extra port with no device attached.
// Todo: Adjust these values when LEDs are enabled.
static const uint16_t ADC_CON1_NOMINAL = 1840;
static const uint16_t ADC_CON2_NOMINAL = 1170;

// Threshold for the change in nominal level indicating connection state change.
//  - +250 indicates that another host is connected.
//  - -250 indicates that (a cable to) another device is connected.
static const uint16_t ADC_CON_NOMINAL_CHANGE_THR = 250;

// Minimum level to decide if host is connected (the port should be measured with
// SR_CTRL_SRC_x enabled.)
static const uint16_t ADC_CON_HOST_CONNECTED = 100;
/** @} */


/**
 * @name    IS31FL3733 RGB LED parameters
 * @{
 */
// Number of ISSI3733 drivers in use (1...16)
#define DRIVER_COUNT                    2

// Hardware address of each driver (refer to ISSI3733 pdf "Table 1 Slave Address" and
// keyboard schematic)
static const uint8_t DRIVER_ADDR[DRIVER_COUNT] = { 0x50, 0x5F };
/** @} */


// Watchdog timeout for debugging aid; if matrix scan is not performed for this period
// long, we jump to the bootloader.
#define WDT_TIMEOUT_MS                  (4 *1000U)  // 4 sec


// The smaller the value, the higher the priority.
#define THREAD_PRIO_USB                 1
#define THREAD_PRIO_MATRIX              2
#define THREAD_PRIO_ADC                 3
#define THREAD_PRIO_RGB                 4
#define THREAD_PRIO_KEYMAP              5
#if THREAD_PRIORITY_MAIN != 7
    #error THREAD_PRIORITY_MAIN != 7
#endif


// Labels from linker script (riot/cpu/cortexm_common/ldscripts/cortexm.ld)
extern uint32_t _sfixed;
extern uint32_t _lrom;
extern uint32_t _erom;

// Consistent with the defintions in riot/sys/newlib_syscalls_default/syscalls.c
extern char _sheap1;
extern char _eheap1;

/**
 * @brief   Initiate a system reset request (SYSRESETREQ) to reset the MCU.
 */
NORETURN static inline void system_reset(void) { NVIC_SystemReset(); }


/**
 * @brief   Reboot to the bootloader.
 */
NORETURN static inline void reboot_to_bootloader(void)
{
    *(uint32_t*)RIOTBOOT_MAGIC_ADDR = RIOTBOOT_MAGIC_NUMBER;
    system_reset();

    // Alternatively, we could cause a watchdog reset even if WDT is not set up.
    // MCLK->APBAMASK.bit.WDT_ = 1;
    // WDT->CLEAR.reg = 0;
    // UNREACHABLE();
}


/**
 * @brief   Enable or disable the system timer for ZTIMER_MSEC.
 */
static inline void ztimer_irq_disable(void)
{
    if ( !irq_is_in() )
        NVIC_DisableIRQ(RTC_IRQn);
        // NVIC_DisableIRQ(timer_config[0].irq);  // for ZTIMER_USEC
}

static inline void ztimer_irq_enable(void)
{
    if ( !irq_is_in() )
        NVIC_EnableIRQ(RTC_IRQn);
        // NVIC_EnableIRQ(timer_config[0].irq);  // for ZTIMER_USEC
}


/**
 * @brief   Retrieve the product serial number "..HMM.*" in the USER page of the device.
 *          Return NULL if not found.
 */
static inline const uint16_t* retrieve_factory_serial(void)
{
    const uint16_t* p = (uint16_t*)(NVMCTRL_USER + sizeof(nvm_user_page_t));
    return *p == 0xFFFFu ? NULL : p;
}


/**
 * @brief   Initialize board specific hardware, including clock, LEDs and std-IO
 */
void board_init(void);

#ifdef __cplusplus
}
#endif

/** @} */
