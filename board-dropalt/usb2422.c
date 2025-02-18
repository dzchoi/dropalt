#include "board.h"                  // for HUB_*, retrieve_factory_serial(), ...
#include "periph/i2c.h"
#include "_usb2422_t.h"             // for struct Usb2422
#include "usb2422.h"
#include "ztimer.h"                 // for ztimer_sleep()



static Usb2422 usb2422_cfg = {
    // configure Usb2422 registers
    .VID.reg = HUB_VENDOR_ID,           // from Microchip 4/19/2018
    .PID.reg = HUB_PRODUCT_ID,          // from Microchip 4/19/2018
    .DID.reg = HUB_DEVICE_VER,          // BCD 01.01
    .CFG1.bit.SELF_BUS_PWR = 1,         // 0=bus-powered, 1=self-powered
    .CFG1.bit.CURRENT_SNS = 0,          // 0=ganged, 1=port-by-port, 2=no sensing
    .CFG1.bit.HS_DISABLE = 1,           // full speed only
    .CFG2.bit.COMPOUND = 1,             // Hub as part of a compound device
    .CFG3.bit.STRING_EN = 1,            // strings enabled
    .NRD.bit.PORT2_NR = 1,              // Keyboard is permantly attached.
    .MAXPB.reg = 0,                     // 0 mA
    .HCMCB.reg = 0,                     // 0 mA
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

static inline void _usbhub_reset(void)
{
    // Pulse reset for at least 1 usec.
    sr_exp_writedata(0, SR_CTRL_HUB_RESET_N);  // reset low
    ztimer_sleep(ZTIMER_USEC, 2);  // t1 for a minimum of 1 us (from table 4-2, USB2422 datasheet)

    sr_exp_writedata(SR_CTRL_HUB_RESET_N, 0);  // reset high to run
    ztimer_sleep(ZTIMER_USEC, 10);
}

// Load HUB configuration data to USB2422.
// Returns true if configured successfully, or false otherwise.
bool usbhub_configure(void)
{
    _usbhub_reset();

    const uint8_t* const src_begin = (const uint8_t*)&usb2422_cfg;
    const size_t src_size = sizeof(usb2422_cfg);
    const uint8_t* const src_end = src_begin + src_size;

    // A block write into USB2422 allows a transfer maximum of 32 data bytes.
    int chunk_size = 32;
    bool status = true;

    uint8_t buffer[2] = {
        0,          // start register number
        chunk_size  // size of the following data
    };

    const i2c_t I2C = I2C_DEV(0);
    for ( const uint8_t* p = src_begin ; status && p < src_end ; p += chunk_size ) {
        if ( chunk_size > src_end - p )
            chunk_size = src_end - p;

        i2c_acquire(I2C);
        status =
            i2c_write_bytes(I2C, USB2422_ADDR, buffer, 2, I2C_NOSTOP) == 0 &&
            i2c_write_bytes(I2C, USB2422_ADDR, p, chunk_size, I2C_NOSTART) == 0;
        i2c_release(I2C);
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
    const uint16_t* serial_str = retrieve_factory_serial();
    if ( serial_str != NULL ) {
        size_t serial_len = 0;
        while ( serial_str[serial_len] != 0u
          && serial_len < sizeof(usb2422_cfg)/sizeof(uint16_t) )
            serial_len++;
        usb2422_cfg.SERSL.reg = serial_len;
        __builtin_memcpy(usb2422_cfg.SERSTR, serial_str, serial_len * sizeof(uint16_t));
    }

    // When keyboard is powered up by manually plugging into the host USB port
    // usbhub_configure() fails sometimes due to the failure with transferring the
    // configuration data over I2C (noise in SMBUS line?).
    while ( !usbhub_configure() )
        ztimer_sleep(ZTIMER_MSEC, 1);
}

// Note:
//  - UP  is upstream device (HOST)
//  - DN1 is downstream device (EXTRA)
//  - DN2 is keyboard (KEYB)

void usbhub_disable_all_ports(void)
{
    sr_exp_writedata(
        SR_CTRL_E_UP_N              // HOST disable
        | SR_CTRL_E_DN1_N           // EXTRA disable
        , SR_CTRL_SRC_1
        | SR_CTRL_SRC_2
    );

    ztimer_sleep(ZTIMER_USEC, 10);
}

void usbhub_set_host_port(uint8_t port)
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

    ztimer_sleep(ZTIMER_USEC, 10);
}

void usbhub_set_extra_port(uint8_t port, bool enable)
{
    if ( port == USB_PORT_UNKNOWN )
        return;

    const uint16_t vbus =
        port == USB_PORT_1 ? SR_CTRL_E_VBUS_1 : SR_CTRL_E_VBUS_2;

    if ( enable )
        sr_exp_writedata(vbus, SR_CTRL_E_DN1_N);
    else
        sr_exp_writedata(SR_CTRL_E_DN1_N, vbus);
}
