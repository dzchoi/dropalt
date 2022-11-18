#include "periph/i2c.h"
#include "xtimer.h"                 // for xtimer_usleep()
#include "_usb2422_t.h"             // for struct Usb2422
#include "usb2422.h"

// This usb2422.c is for initializing USB2422 hub and for enabling and disabling the
// extra USB port based on the voltage measurements from adc_thread.



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
        __builtin_memcpy(usb2422_cfg.SERSTR, serial_str, serial_len * sizeof(uint16_t));
    }
}

static inline void _usbhub_reset(void)
{
    // Pulse reset for at least 1 usec.
    sr_exp_writedata(0, SR_CTRL_HUB_RESET_N);  // reset low
    xtimer_usleep(2);  // t1 for a minimum of 1 us (from table 4-2, USB2422 datasheet)

    sr_exp_writedata(SR_CTRL_HUB_RESET_N, 0);  // reset high to run
}

// Load HUB configuration data to USB2422.
// Returns true if configured successfully, or false otherwise.
bool usbhub_configure(void)
{
    _usbhub_reset();
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

    // connect signal
    sr_exp_writedata(SR_CTRL_HUB_CONNECT, 0);

    // Prepare the HUB configuration data
    retrieve_factory_serial();

    // When the keyboard is powered up by manually plugging in to a host port instead of
    // powering up the host with the keyboard already connected, usbhub_configure() can
    // fail sometimes. HUB seems unresponsive to the configuration data sent over I2C.
    while ( !usbhub_configure() )
        xtimer_msleep(1);
}

// Note:
//   - UP  is upstream device (HOST)
//   - DN1 is downstream device (EXTRA)
//   - DN2 is keyboard (KEYB)

void usbhub_disable_all_ports(void)
{
    sr_exp_writedata(
        SR_CTRL_SRC_1               // USBC-1 available for test
        | SR_CTRL_SRC_2             // USBC-2 available for test
        | SR_CTRL_E_UP_N            // HOST disable
        | SR_CTRL_E_DN1_N           // EXTRA disable
        , 0
    );
    xtimer_usleep(10);
}

void usbhub_enable_host_port(uint8_t port)
{
    if ( port == USB_PORT_UNKNOWN )
        return;

    if ( port == USB_PORT_1 )
        sr_exp_writedata(
            SR_CTRL_S_DN1           // EXTRA to USBC-2
            | SR_CTRL_SRC_1         // HOST on USBC-1
            ,
            SR_CTRL_E_UP_N          // HOST enable
            | SR_CTRL_S_UP          // HOST to USBC-1
            | SR_CTRL_SRC_2         // EXTRA available on USBC-2
        );
    else
        sr_exp_writedata(
            SR_CTRL_S_UP            // HOST to USBC-2
            | SR_CTRL_SRC_2         // HOST on USBC-2
            ,
            SR_CTRL_E_UP_N          // HOST enable
            | SR_CTRL_S_DN1         // EXTRA to USBC-1
            | SR_CTRL_SRC_1         // EXTRA available on USBC-1
        );
    xtimer_usleep(10);
}

void usbhub_enable_extra_port(uint8_t port, bool yes_no)
{
    if ( port == USB_PORT_UNKNOWN )
        return;

    const uint16_t vbus =
        port == USB_PORT_1 ? SR_CTRL_E_VBUS_1 : SR_CTRL_E_VBUS_2;

    if ( yes_no )
        sr_exp_writedata(vbus, SR_CTRL_E_DN1_N);
    else
        sr_exp_writedata(SR_CTRL_E_DN1_N, vbus);
}
