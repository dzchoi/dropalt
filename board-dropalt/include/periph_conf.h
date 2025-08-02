/**
 * @ingroup     board-dropalt
 * @{
 *
 * @file
 * @brief       Configuration of CPU peripherals for Drop Alt
 *
 * @author      dzchoi <19231860+dzchoi@users.noreply.github.com>
 */

#pragma once

#include "periph_cpu.h"



#ifdef __cplusplus
extern "C" {
#endif

// Beware: The g++ does support "designated initializers", but unlike C99 the members
//   should appear in the same order as in the struct definition.

/**
 * @brief   Use the external oscillator to source all fast clocks.
 *          This allows us to use the buck voltage regulator for
 *          maximum power efficiency, but limits the maximum clock
 *          frequency to 12 MHz.
 */
#ifndef USE_XOSC_ONLY
#define USE_XOSC_ONLY       (0)
#endif

/**
 * @name    external Oscillator (XOSC0/XOSC1) configuration
 * @{
 */
// Beware that USE_XOSC_ONLY == 0 implies USE_XOSC == 0 and will not activate 
// GCLK_SOURCE_XOSC0 regardless of this definition.
#define XOSC0_FREQUENCY     MHZ(16)
/** @} */

/**
 * @name    desired core clock frequency
 * @{
 */
#ifndef CLOCK_CORECLOCK
#define CLOCK_CORECLOCK     MHZ(120)
#endif
/** @} */

/**
 * @name    32kHz Oscillator configuration
 * @{
 */
#define EXTERNAL_OSC32_SOURCE                    0
#define ULTRA_LOW_POWER_INTERNAL_OSC_SOURCE      1
// One of those should be 1 when USE_XOSC_ONLY == 0, and GCLK1 (SAM0_GCLK_32KHZ) will 
// be set to source from the corresponding oscillator.
/** @} */

/**
 * @brief Enable the internal DC/DC converter
 *        The board is equipped with the necessary inductor.
 */
#define USE_VREG_BUCK       (1)

// GCKL2 (SAM0_GCLK_TIMER) clock frequency
#define USE_DFLL_FOR_GCLK_TIMER     // Otherwise will use DPLL0.
#define GCLK_TIMER_HZ       MHZ(8)  // For TC0, it will be prescaled down to 1 MHz.

// GCLK frequencies
//   - SAM0_GCLK_MAIN:   120 MHz main clock
//   - SAM0_GCLK_32KHZ:  32 kHz clock
//   - SAM0_GCLK_TIMER:  8 MHz clock for ZTIMER_USEC (prescaled to 1 MHz)
//   - SAM0_GCLK_PERIPH: 48 MHz (DFLL) clock


/**
 * @name Timer peripheral configuration
 * @{
 */
static const tc32_conf_t timer_config[] = {
    {   /* Timer 0 - System Clock, channel used for ZTIMER_USEC. */
        .dev            = TC0,
        .irq            = TC0_IRQn,
        .mclk           = &MCLK->APBAMASK.reg,
        .mclk_mask      = MCLK_APBAMASK_TC0 | MCLK_APBAMASK_TC1,
        .gclk_id        = TC0_GCLK_ID,
        .gclk_src       = SAM0_GCLK_TIMER,  // 8MHz
        .flags          = TC_CTRLA_MODE_COUNT32,
    },
    // {   /* Timer 1 - unused, no prescaler by div32 available. */
    //     .dev            = TC4,
    //     .irq            = TC4_IRQn,
    //     .mclk           = &MCLK->APBCMASK.reg,
    //     .mclk_mask      = MCLK_APBCMASK_TC4,
    //     .gclk_id        = TC4_GCLK_ID,
    //     .gclk_src       = SAM0_GCLK_32KHZ,  // 32KHz
    //     .flags          = TC_CTRLA_MODE_COUNT32,  // count up by default
    // }
};

/* Timer 0 configuration */
#define TIMER_0_CHANNELS    2
#define TIMER_0_ISR         isr_tc0

/* Timer 1 configuration */
// #define TIMER_1_CHANNELS    2
// #define TIMER_1_ISR         isr_tc4

#define TIMER_NUMOF         ARRAY_SIZE(timer_config)
/** @} */


/**
 * @name ADC Configuration
 * @{
 */

/* ADC Default values */
#define ADC_GCLK_SRC        SAM0_GCLK_PERIPH          // 48MHz
#define ADC_PRESCALER       ADC_CTRLA_PRESCALER_DIV8  // prescaled down to 6MHz

// ADC sampling time to charge the sampling capacitor (HW-designed)
#define ADC_SAMPLELEN       ADC_SAMPCTRL_SAMPLEN(34)  // = 45 cycles when 8MHz

// Number of samples to average per each measurement
// So, one measurement in 12-bit resolution will take (34+12)*128/6 us = 981 us = ~1 ms.
#define ADC_SAMPLENUM       ADC_AVGCTRL_SAMPLENUM_128

// See for Vref https://www.powerelectronictips.com/choosing-reference-voltage-adc-2/
#define ADC_REF_DEFAULT     ADC_REFCTRL_REFSEL_INTREF  // use internal reference voltage.
// Todo: ADC_REFCTRL_REFSEL_INTVCC1?
#define ADC_NEG_INPUT       ADC_INPUTCTRL_MUXNEG(0x18u)

static const adc_conf_chan_t adc_channels[] = {
    /* port, pin, muxpos, dev */
    { ADC0_INPUTCTRL_MUXPOS_PB00, ADC0 },  // ADC 5V
    { ADC0_INPUTCTRL_MUXPOS_PB02, ADC0 },  // ADC CON1
    { ADC0_INPUTCTRL_MUXPOS_PB01, ADC0 }   // ADC CON2
};

#define ADC_NUMOF           ARRAY_SIZE(adc_channels)

#define ADC0_IRQ            ADC0_1_IRQn  // Irq for ADC0_RESRDY
#define ADC_ISR             isr_adc0_1
/** @} */


/**
 * @name SPI configuration
 * @{
 */
static const spi_conf_t spi_config[] = {
    {   // Used for 595 shift register
        .dev      = &SERCOM2->SPI,
        .miso_pin = GPIO_UNDEF,             // Not using Input
        .mosi_pin = GPIO_PIN(PA, 12),       // SPI Data Out (MCU.SERCOMx.PAD[0] to
                                            // Shift Register's SER)
        .clk_pin  = GPIO_PIN(PA, 13),       // SPI Serial Clock (MCU.SERCOMx.PAD[1] to
                                            // Shift Register's SRCLK)
        .miso_mux = GPIO_MUX_C,
        .mosi_mux = GPIO_MUX_C,
        .clk_mux  = GPIO_MUX_C,
        // Maybe, we can reuse DIPO = 3 as an unused PIN.
        .miso_pad = SPI_PAD_MISO_3,         // Data In PAD[3] - Not used.
                                            // Configured away from DOPO.
        .mosi_pad = SPI_PAD_MOSI_0_SCK_1,   // Data Output PAD[0], Serial Clock Pad[1]
        .gclk_src = SAM0_GCLK_PERIPH,
#ifdef MODULE_PERIPH_DMA
        // Cannot use DMA as both TX and RX should be enabled and we are not using RX.
        .tx_trigger = DMA_TRIGGER_DISABLED,
        .rx_trigger = DMA_TRIGGER_DISABLED,
#endif
    }
};

#define SPI_NUMOF           ARRAY_SIZE(spi_config)

/**
 * The 595 Shift Register expands available hardware output lines to control additional
 * peripherals. It uses four lines (SER, SRCLK, RCLK, and OE_N) from the MCU to provide
 * 16 output lines.
 */
#define SR_EXP_SERCOM       SPI_DEV(0)          // SERCOM port for Shift Register
#define SR_EXP_SPI_FREQ     SPI_CLK_1MHZ        // spi to Shift Register
#define SR_EXP_RCLK         GPIO_PIN(PB, 14)    // works like a SPI chip-select pin
#define SR_EXP_OE_N         GPIO_PIN(PB, 15)
/** @} */


/**
 * @name I2C configuration
 * @{
 */
static const i2c_conf_t i2c_config[] = {
    {   // Used for USB2422 HUB
        // Beware that I2C clk must be high at the time USB2422 reset release to signal
        // SMB configuration.
        .dev      = &(SERCOM0->I2CM),
        .speed    = I2C_SPEED_NORMAL,       // Standard mode (Sm) up to 100 Kbps
        .scl_pin  = GPIO_PIN(PA, 9),        // SERCOM PAD[1] - SCL
        .sda_pin  = GPIO_PIN(PA, 8),        // SERCOM PAD[0] - SDA
        .mux      = GPIO_MUX_C,
        .gclk_src = SAM0_GCLK_PERIPH,
        .flags    = I2C_FLAG_RUN_STANDBY,
#ifdef MODULE_PERIPH_DMA
        // Note: Using .tx_trigger = SERCOM0_DMAC_ID_TX for I2C DMA transfer may
        // occasionally hang, when powering the keyboard via a USB cable. The I2C
        // transfer to USB2422 might fail if the device isn't fully initialized, which
        // can cause the DMA transaction to stall.
        // This can be worked around by modifying dma_wait() to use a timed wait and
        // retrying the transfer (which may take several hundred milliseconds). However,
        // using non-DMA I2C is acceptable here, as the transfer only occurs once at
        // power-up and incurs a ~30 ms overhead compared to DMA.
        .tx_trigger = DMA_TRIGGER_DISABLED,
        .rx_trigger = DMA_TRIGGER_DISABLED,
#endif
    },
    {   // Used for IS31FL3733 LED driver
        .dev      = &(SERCOM1->I2CM),
        .speed    = I2C_SPEED_FAST_PLUS,    // Fast mode Plus (Fm+) up to 1 Mbps
        // ??? .speed    = KHZ(580),
        .scl_pin  = GPIO_PIN(PA, 17),       // SERCOM PAD[1] - SCL
        .sda_pin  = GPIO_PIN(PA, 16),       // SERCOM PAD[0] - SDA
        .mux      = GPIO_MUX_C,
        .gclk_src = SAM0_GCLK_PERIPH,
        .flags    = I2C_FLAG_RUN_STANDBY,
#ifdef MODULE_PERIPH_DMA
        .tx_trigger = SERCOM1_DMAC_ID_TX,
        .rx_trigger = SERCOM1_DMAC_ID_RX,   // Use DMA_TRIGGER_DISABLED to disable DMA
#endif
    }
    // i2c1 Set   ~Result     PWM Time (2x Drivers)
    // 1000000    1090000
    // 900000     1000000     3.82 ms
    // 800000     860000
    // 700000     750000
    // 600000     630000
    // 580000     615000      6.08 ms
    // 500000     522000
};

#define I2C_NUMOF          ARRAY_SIZE(i2c_config)
/** @} */


/**
 * @name USB peripheral configuration
 * @{
 */
static const sam0_common_usb_config_t sam_usbdev_config[] = {
    {
        .dm       = GPIO_PIN(PA, 24),       // USB D-
        .dp       = GPIO_PIN(PA, 25),       // USB D+
        .d_mux    = GPIO_MUX_H,             // USB SOF_1KHz output
        .device   = &USB->DEVICE,
        .gclk_src = SAM0_GCLK_PERIPH,
    }
};

#define USB2422_ADDR    0x2C                // I2C device address, one instance
#define USB2422_ACTIVE  GPIO_PIN(PA, 18)    // USB2422 SUSP_IND(?) pin
/** @} */


/**
 * @name RTT configuration
 * @{
 */
#ifndef RTT_FREQUENCY
#define RTT_FREQUENCY       (32768U)    /* in Hz. For changes see `rtt.c` */
#endif
// The minimum time (in ticks) that can be waited by RTT, which is used for ZTIMER_MSEC.
// See the comment in _ztimer_periph_rtt_set() in riot/sys/ztimer/periph_rtt.c.
// Beware that smaller values would cause a ztimer (and all other ztimers afterwards)
// being missed to expire. Even if ztimer is not explicitly set for this small period
// (e.g. using 0 to trigger it immediately), this can be still used when ztimer updating
// its timer list internally.
#define RTT_MIN_OFFSET      (10U)  // ~0.305 ms
/** @} */


#if 0
/**
 * @name UART configuration
 * @{
 */
static const uart_conf_t uart_config[] = {
    {   // debug util
        .dev      = &SERCOM3->USART,
        .rx_pin   = GPIO_PIN(PA, 16),
        .tx_pin   = GPIO_PIN(PA, 17),
#ifdef MODULE_PERIPH_UART_HW_FC
        .rts_pin  = GPIO_UNDEF,
        .cts_pin  = GPIO_UNDEF,
#endif
        .mux      = GPIO_MUX_D,
        .rx_pad   = UART_PAD_RX_1,
        .tx_pad   = UART_PAD_TX_0,
        .flags    = UART_FLAG_NONE,
        .gclk_src = SAM0_GCLK_PERIPH,
    },
};

/* interrupt function name mapping */
#define UART_0_ISR          isr_sercom3_2
#define UART_0_ISR_TX       isr_sercom3_0

#define UART_NUMOF          ARRAY_SIZE(uart_config)
/** @} */

/**
 * @name DAC configuration
 * @{
 */
#define DAC_CLOCK           SAM0_GCLK_1MHZ
                            /* use Vcc as reference voltage */
#define DAC_VREF            DAC_CTRLB_REFSEL_AVCC
/** @} */
#endif


/* key matrix size */
#define MATRIX_ROWS 5
#define MATRIX_COLS 15
#define NUM_MATRIX_SLOTS (MATRIX_ROWS * MATRIX_COLS)

/* Port and Pin definition of key row hardware configuration */
static const gpio_t row_pins[MATRIX_ROWS] = {  // for input
    GPIO_PIN(PA, 0),
    GPIO_PIN(PA, 1),
    GPIO_PIN(PA, 2),
    GPIO_PIN(PA, 3),
    GPIO_PIN(PA, 4)
};

/* Port and Pin definition of key column hardware configuration */
static const gpio_t col_pins[MATRIX_COLS] = {  // for output
    GPIO_PIN(PB, 4),
    GPIO_PIN(PB, 5),
    GPIO_PIN(PB, 6),
    GPIO_PIN(PB, 7),
    GPIO_PIN(PB, 8),
    GPIO_PIN(PB, 9),
    GPIO_PIN(PB, 10),
    GPIO_PIN(PB, 11),
    GPIO_PIN(PB, 12),
    GPIO_PIN(PB, 13),
    GPIO_PIN(PA, 5),
    GPIO_PIN(PA, 6),
    GPIO_PIN(PA, 7),
    GPIO_PIN(PA, 10),
    GPIO_PIN(PA, 11),
};

#ifdef __cplusplus
}
#endif

/** @} */
