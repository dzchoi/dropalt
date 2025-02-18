#include "periph_conf.h"
#include "periph/gpio.h"
#include "periph/spi.h"
#include "sr_exp.h"



sr_exp_t sr_exp_data;

void sr_exp_writedata(uint16_t set_bits, uint16_t clear_bits)
{
    // SPI_MODE_3: CPOL == 1 & CPHA == 1.
    spi_acquire(SR_EXP_SERCOM, SR_EXP_RCLK, SPI_MODE_3, SR_EXP_SPI_FREQ);

    // No need for this???
    // while ( !(((SercomSpi*)spi_config[SR_EXP_SERCOM].dev)->INTFLAG.bit.DRE) ) {}

    sr_exp_data.reg |= set_bits;
    sr_exp_data.reg &= ~clear_bits;

    // MSBs are sent first as CTRLA.bit.DORD == 0.
    spi_transfer_byte(SR_EXP_SERCOM, SR_EXP_RCLK, true, sr_exp_data.high);  // bits 15-8
    spi_transfer_byte(SR_EXP_SERCOM, SR_EXP_RCLK, false, sr_exp_data.low);  // bits 7-0

    spi_release(SR_EXP_SERCOM);
}

void sr_exp_init(void) {
    // Set up MCU Shift Register pins
    gpio_init(SR_EXP_RCLK, GPIO_OUT);
    gpio_init(SR_EXP_OE_N, GPIO_OUT);

    // Disable Shift Register output
    gpio_set(SR_EXP_OE_N);

    // Note: spi_init(SR_EXP_SERCOM) is automatically called by cpu_init().

    sr_exp_writedata(
        SR_CTRL_E_UP_N
        | SR_CTRL_S_DN1
        | SR_CTRL_E_DN1_N
        | SR_CTRL_SRC_1
        | SR_CTRL_SRC_2
        | SR_CTRL_IRST,
        0 );

    // Enable Shift Register output
    gpio_clear(SR_EXP_OE_N);
}
