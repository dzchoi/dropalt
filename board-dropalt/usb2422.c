#include <stdbool.h>
#include <string.h>                 // memcpy()
#define ENABLE_DEBUG    (0)
#include "debug.h"
#include "periph/gpio.h"
#include "periph/i2c.h"
#include "periph/wdt.h"
#include "xtimer.h"

#include "_usb2422_t.h"             // Usb2422 type
// #include "adc_thread.h"             // is_host_on_con1()
#include "sr_exp.h"
#include "usb2422.h"

// This usb2422.c is for initializing USB2422 hub and for enabling and disabling the
// extra USB port based on the voltage measurements from adc_thread.



volatile uint8_t usb_extra_port;

#ifndef MD_BOOTLOADER
volatile int8_t usb_extra_state;
volatile uint8_t usb_extra_manual;
#endif

// periph_conf.h defines _sfixed, _erom, and BOOTLOADER_SERIAL_MAX_SIZE.

static Usb2422 usb2422_cfg = {
    // configure Usb2422 registers
    .VID.reg = HUB_VENDOR_ID,           // from Microchip 4/19/2018
    .PID.reg = HUB_PRODUCT_ID,          // from Microchip 4/19/2018
    .DID.reg = HUB_DEVICE_VER,          // BCD 01.01
    .CFG1.bit.SELF_BUS_PWR = 0,         // 0=bus-powered, 1=self-powered
    .CFG1.bit.CURRENT_SNS = 0,          // 0=ganged, 1=port-by-port, 2=no sensing
    .CFG1.bit.HS_DISABLE = 1,           // full speed only
    .CFG2.bit.COMPOUND = 1,             // Hub as part of a compound device
    .CFG3.bit.STRING_EN = 1,            // strings enabled
    .NRD.bit.PORT2_NR = 1,              // Keyboard is permantly attached.
    .MAXPB.reg = (500 >> 1),            // 500mA together (Keyboard will report 0mA.)
    .HCMCB.reg = 0,                     // 0mA
    .MFRSL.reg = sizeof(HUB_MANUFACTURER) - 1,  // exclude the trailing '\0'
    .PRDSL.reg = sizeof(HUB_PRODUCT) - 1,
    .SERSL.reg = sizeof(HUB_NO_SERIAL) - 1,
    .MFRSTR = u"" HUB_MANUFACTURER,     // in utf-16 format
    .PRDSTR = u"" HUB_PRODUCT,
    .SERSTR = u"" HUB_NO_SERIAL,
    // .BOOSTUP.bit.BOOST = 3,          // upstream port
    // .BOOSTDOWN.bit.BOOST1 = 0,       // extra port
    // .BOOSTDOWN.bit.BOOST2 = 2,       // MCU is close
    .STCD.bit.USB_ATTACH = 1
};

#ifdef MD_BOOTLOADER
// In production, this field is found, modified, and offset noted as the last 32-bit
// word in the bootloader partition. The offset allows the application to use the
// factory-programmed serial (which may differ from the physical serial label).
// Serial number reported stops before the first found space character or when max
// size is reached.
__attribute__((__aligned__(4)))
uint16_t SERNAME[BOOTLOADER_SERIAL_MAX_SIZE] = u"MDHUBBOOTL0000000000";
// NOTE: Serial replacer will not write a string longer than given here as a
//   precaution, so give enough space as needed and adjust BOOTLOADER_SERIAL_MAX_SIZE
//   to match amount given.
#endif

static void retrieve_factory_serial(void)
{
#ifndef MD_BOOTLOADER
    const uint16_t* serial_str = NULL;  // Use HUB_NO_SERIAL that is already engraved
    uint8_t serial_len = 0;
    const uint32_t pointer_to_serial = (uint32_t)&_sfixed - sizeof(uint16_t*);
#else
    const uint16_t* serial_str = SERNAME;  // Use SERNAME from this file by default
    uint8_t serial_len = sizeof(SERNAME) / sizeof(uint16_t);
    const uint32_t pointer_to_serial = (uint32_t)&_erom - sizeof(uint16_t*);
#endif
    // Address of bootloader's serial number if available
    const uint32_t serial_addr = *(uint32_t*)pointer_to_serial;

    // Check for valid serial address
    if ( serial_addr % sizeof(uint16_t*) == 0 && serial_addr < pointer_to_serial ) {
        serial_str = (uint16_t*)(serial_addr);
        for ( serial_len = 0;
              serial_str[serial_len] != 0  // or isgraph(serial_str[serial_len]) != 0
                  && serial_len < BOOTLOADER_SERIAL_MAX_SIZE;
              serial_len++ );
    }

    if ( serial_str != NULL ) {
        usb2422_cfg.SERSL.reg = serial_len;
        memcpy(usb2422_cfg.SERSTR, serial_str, serial_len * sizeof(uint16_t));
    }
}

static inline void usbhub_reset(void)
{
    // Pulse reset for at least 1 usec.
    sr_exp_writedata(0, SR_CTRL_HUB_RESET_N);  // reset low
    xtimer_usleep(2);  // t1 for a minimum of 1 us (from table 4-2, USB2422 datasheet)

    sr_exp_writedata(SR_CTRL_HUB_RESET_N, 0);  // reset high to run
}

// Load HUB configuration data to USB2422.
// Returns true if configured successfully, or false otherwise.
static bool usbhub_configure(void)
{
    usbhub_reset();
    xtimer_usleep(10);

    bool status = true;
    const size_t data_size = sizeof(Usb2422);
    const size_t chunk_size = 32;
    assert( data_size % chunk_size == 0 );  // Todo: assert() from RIOT?

    uint8_t buffer[2] = {
        0,          // start register number
        chunk_size  // size of the following data
    };

    const uint8_t* const base = (const uint8_t*)&usb2422_cfg;
    for ( const uint8_t* src = base ; status && src < &base[data_size] ; src += chunk_size ) {
        i2c_acquire(I2C_DEV(0));
        status =
            i2c_write_bytes(I2C_DEV(0), USB2422_ADDR, buffer, 2, I2C_NOSTOP) == 0 &&
            i2c_write_bytes(I2C_DEV(0), USB2422_ADDR, src, chunk_size, I2C_NOSTART) == 0;
        i2c_release(I2C_DEV(0));
        buffer[0] += chunk_size;
    }

    return status;
}

void usbhub_init(void)
{
    gpio_init(USB2422_ACTIVE, GPIO_IN);

    // I2C clk must be high at USB2422 reset release time to signal SMB configuration.
    // i2c0_init();

    // connect signal, reset high
    sr_exp_writedata(SR_CTRL_HUB_CONNECT | SR_CTRL_HUB_RESET_N, 0);

    // Can't use xtimer_usleep() here, since this function is called from board_init().
    // clk_busy_wait_us(100);  // Is it needed?

    // Prepare the HUB configuration data
    retrieve_factory_serial();

    usb_extra_port = USB_PORT_UNKNOWN;

#ifndef MD_BOOTLOADER
    usb_extra_manual = 0;
    usb_extra_state = USB_EXTRA_STATE_DISABLED;
#endif
}

static inline int usbhub_active(void)
{
    return gpio_read(USB2422_ACTIVE);
}

// Todo:
bool is_host_on_con1(void) { return true; }

// Waits for host detected on CON1 or CON2, and attaches the hub to the host.
// Required to be called from (a sub-function of) main(), which is after xtimer_init()
// is done.
// Todo: Automatic calling of usbhub_wait_for_host(),
//   - when the current v_con (not v_extra) is down (i.e. in panic mode), or
//   - when USB is disconnected (usbus.state == USBUS_STATE_DISCONNECT, but not working
//     for now), or
//   - when USB 5V is lost.
// Todo: If the USB in the desired port is suspended when switchover, wake it up first.
void usbhub_wait_for_host(uint8_t desired_host_port)
{
    // Note:
    //   - UP  is upstream device (HOST)
    //   - DN1 is downstream device (EXTRA)
    //   - DN2 is keyboard (KEYB)

    usb_extra_port = USB_PORT_UNKNOWN;  // to be determined below
    static const uint32_t RETRY_TIMER_US = 2 *US_PER_SEC;

    // Detect host by voltage on ADC_LINE_CON1 and ADC_LINE_CON2.
    uint32_t time_to_retry = xtimer_now_usec();
    do {
        wdt_kick();
        const uint32_t now = xtimer_now_usec();

        if ( (int32_t)(now - time_to_retry) >= 0 ) {
            // Indicate on the LED that we're alive. We will stay in this loop when
            // plugged in a suspended host, until the host wakes up.
            LED0_ON;

            sr_exp_writedata(
                SR_CTRL_SRC_1               // USBC-1 available for test
                | SR_CTRL_SRC_2             // USBC-2 available for test
                | SR_CTRL_E_UP_N            // HOST disable
                | SR_CTRL_E_DN1_N           // EXTRA disable
                ,
                SR_CTRL_E_VBUS_1            // USBC-1 disable full power I/O
                | SR_CTRL_E_VBUS_2          // USBC-2 disable full power I/O
            );
            xtimer_usleep(10);

            if ( desired_host_port == USB_PORT_UNKNOWN )
                desired_host_port = is_host_on_con1() ? USB_PORT_1 : USB_PORT_2;
            if ( desired_host_port == USB_PORT_1 ) {
                sr_exp_writedata(
                    SR_CTRL_S_DN1           // EXTRA to USBC-2
                    | SR_CTRL_SRC_1         // HOST on USBC-1
                    | SR_CTRL_E_VBUS_1      // USBC-1 enable full power I/O
                    ,
                    SR_CTRL_S_UP            // HOST to USBC-1
                    | SR_CTRL_SRC_2         // EXTRA available on USBC-2
                    | SR_CTRL_E_VBUS_2      // USBC-2 disable full power I/O
                    | SR_CTRL_E_UP_N        // HOST enable
                );
            } else {
                sr_exp_writedata(
                    SR_CTRL_S_UP            // HOST to USBC-2
                    | SR_CTRL_SRC_2         // HOST on USBC-2
                    | SR_CTRL_E_VBUS_2      // USBC-2 enable full power I/O
                    ,
                    SR_CTRL_S_DN1           // EXTRA to USBC-1
                    | SR_CTRL_SRC_1         // EXTRA available on USBC-1
                    | SR_CTRL_E_VBUS_1      // USBC-1 disable full power I/O
                    | SR_CTRL_E_UP_N        // HOST enable
                );
            }
            xtimer_usleep(10);

            // When keyboard boots up by plugging in to the host USB port manually,
            // usbhub_configure can fail sometimes (HUB not responding to the
            // configuration data sent over I2C) until the voltage (or clock?) is stable.
            while ( !usbhub_configure() )
                xtimer_msleep(100);

            // Try the given desired port first. Then try either port that has a higher
            // voltage afterwards.
            desired_host_port = USB_PORT_UNKNOWN;
            time_to_retry = now + RETRY_TIMER_US;
            LED0_OFF;
        }

        xtimer_msleep(1);
    } while ( !usbhub_active() );

    // Determine the extra port by looking at the last call of sr_exp_writedata().
    usb_extra_port = sr_exp_data.bit.S_DN1 ? USB_PORT_2 : USB_PORT_1;
    usb_extra_state = USB_EXTRA_STATE_DISABLED;
}

#ifndef MD_BOOTLOADER
void usbhub_set_extra_state(int8_t state)
{
    const uint16_t vbus =
        (usb_extra_port == USB_PORT_1 ? SR_CTRL_E_VBUS_1 : SR_CTRL_E_VBUS_2);

    if ( state == USB_EXTRA_STATE_ENABLED )
        sr_exp_writedata(vbus, SR_CTRL_E_DN1_N);
    else  // state == USB_EXTRA_STATE_DISABLED or USB_EXTRA_STATE_FORCED_DISABLED
        sr_exp_writedata(SR_CTRL_E_DN1_N, vbus);

#if !defined(NO_DEBUG)  // && defined(CONSOLE_ENABLE)
    switch ( state ) {
        case USB_EXTRA_STATE_FORCED_DISABLED:
            DEBUG("USB: extra port forced disabled\n");
            break;

        case USB_EXTRA_STATE_DISABLED:
            DEBUG("USB: extra port disabled\n");
#   ifdef USE_MASSDROP_CONFIGURATOR
            // Todo: Reconsider gcr_breathe.
            if ( led_animation_breathing )
                gcr_breathe = gcr_desired;
#   endif
            break;

        case USB_EXTRA_STATE_ENABLED:
            DEBUG("USB: extra port enabled\n");
            break;
    }
#endif

    usb_extra_state = state;
}

void usbhub_handle_extra_device(bool is_extra_plugged_in)
{
    // As it can be called (periodically) from the start of "adc_thread", we need to
    // ensure that usbhub_wait_for_host() is done before.
    if ( usb_extra_port == USB_PORT_UNKNOWN )
        return;

    switch ( usb_extra_state ) {
        case USB_EXTRA_STATE_FORCED_DISABLED:
            // Detect unplug and reset state to disabled
            if ( !is_extra_plugged_in )
                usb_extra_state = USB_EXTRA_STATE_DISABLED;
            usb_extra_manual = 0;
            break;  // Do nothing even if unplug detected

        case USB_EXTRA_STATE_DISABLED:
            if ( is_extra_plugged_in || usb_extra_manual )
                usbhub_set_extra_state(USB_EXTRA_STATE_ENABLED);
            break;

        case USB_EXTRA_STATE_ENABLED:
            if ( !is_extra_plugged_in && !usb_extra_manual )
                usbhub_set_extra_state(USB_EXTRA_STATE_DISABLED);
            break;
    }
}
#endif  // MD_BOOTLOADER
