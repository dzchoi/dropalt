#include "board.h"              // for DRIVER_COUNT, DRIVER_ADDR, ...
#include "is31fl3733.h"
#include "periph/i2c.h"         // for i2c_write_bytes() and i2c_read_bytes()
#include "sr_exp.h"             // for sr_exp_writedata()
#include "ztimer.h"             // for ztimer_sleep()



#define I2C I2C_DEV(1)

#define REG_COMMAND                 0xFD
#define REG_COMMAND_WRITE_LOCK      0xFE
#define REG_INTERRUPT_MASK          0xF0
#define REG_INTERRUPT_STATUS        0xF1

#define SEL_UNLOCK                  0xC5

#define SEL_PG0_LED_CONTROL         0x00
#define SEL_PG1_PWM                 0x01
#define SEL_PG2_AUTO_BREATH         0x02
#define SEL_PG3_FUNCTION            0x03

#define PG3_CONFIGURATION           0x00
#define PG3_GLOBAL_CURRENT          0x01
#define PG3_SW_PULL_UP              0x0F
#define PG3_CS_PULL_DOWN            0x10
#define PG3_RESET                   0x11



// Power-up default state for command register is 0.
uint8_t g_command_register[DRIVER_COUNT];

// PG0 LED on/off registers (initialized with all 0's)
uint8_t g_led_on_off_registers[DRIVER_COUNT][LED_CONTROL_REGISTER_COUNT];

// PG1 PWM registers (initialized with all 0's)
uint8_t g_pwm_registers[DRIVER_COUNT][PWM_REGISTER_COUNT];
uint8_t g_pwm_registers_need_update[DRIVER_COUNT][PWM_REGISTER_COUNT / 8];

extern char __STATIC_ASSERT__[1/( PWM_REGISTER_COUNT % 8 == 0 )];



// If transaction fails function returns false.
static void _is31_read_register(uint8_t addr, uint8_t reg, uint8_t* pdata)
{
    if ( i2c_write_bytes(I2C, addr, &reg, 1, 0) != 0
      || i2c_read_bytes(I2C, addr, pdata, 1, 0) != 0 )
    {
        assert( false );
    }
}

static void _is31_write_register(uint8_t addr, uint8_t reg, uint8_t data)
{
    uint8_t twi_transfer_buffer[] = { reg, data };
    if ( i2c_write_bytes(I2C, addr, twi_transfer_buffer, 2, 0) != 0 )
        assert( false );
}

// Write into consecutive registers.
static void _is31_write_registers(
    uint8_t addr, uint8_t reg, const uint8_t pdata[], size_t n)
{
    if ( i2c_write_bytes(I2C, addr, &reg, 1, I2C_NOSTOP) != 0
      || i2c_write_bytes(I2C, addr, pdata, n, I2C_NOSTART) != 0 )
    {
        assert( false );
    }
}

static void _is31_set_command_register(size_t driver, uint8_t page)
{
    if ( g_command_register[driver] != page ) {
        const uint8_t addr = DRIVER_ADDR[driver];
        _is31_write_register(addr, REG_COMMAND_WRITE_LOCK, SEL_UNLOCK);
        _is31_write_register(addr, REG_COMMAND, page);
        g_command_register[driver] = page;
    }
}

static void _is31_write_led_on_off_registers(size_t driver)
{
    _is31_set_command_register(driver, SEL_PG0_LED_CONTROL);
    _is31_write_registers(DRIVER_ADDR[driver],
        0, g_led_on_off_registers[driver], sizeof(g_led_on_off_registers[0]));
}

static void _is31_write_pwm_registers(size_t driver)
{
#ifndef MODULE_PERIPH_DMA
#   error _is31_write_pwm_registers() is implemented only with MODULE_PERIPH_DMA enabled.
#endif
    _is31_set_command_register(driver, SEL_PG1_PWM);

    const uint8_t addr = DRIVER_ADDR[driver];
    size_t n = 0;  // number of consecutive pwm registers to update.

    for ( size_t i = 0 ; i < sizeof(g_pwm_registers_need_update[0]) ; i++ ) {
        uint8_t flags = g_pwm_registers_need_update[driver][i];
        g_pwm_registers_need_update[driver][i] = 0;

        for ( size_t j = 0 ; j < 8 ; j++, flags >>= 1 )
            if ( flags % (uint8_t)2 != 0 )
                n++;
            else if ( n > 0 ) {
                const uint8_t reg = i*8 + j - n;
                _is31_write_registers(addr, reg, &g_pwm_registers[driver][reg], n);
                n = 0;
            }
    }

    if ( n > 0 ) {
        const uint8_t reg = PWM_REGISTER_COUNT - n;
        _is31_write_registers(addr, reg, &g_pwm_registers[driver][reg], n);
    }
}

// Read reset register to intialize all registers to their default values, so Control
// registers and PWM registers will reset to all 0.
static void _is31_reset_registers(size_t driver)
{
    uint8_t tmp;
    _is31_set_command_register(driver, SEL_PG3_FUNCTION);
    _is31_read_register(DRIVER_ADDR[driver], PG3_RESET, &tmp);

    g_command_register[driver] = SEL_PG0_LED_CONTROL;  // power-up default
    __builtin_memset(g_led_on_off_registers[driver], 0,
        sizeof(g_led_on_off_registers[0]));
    __builtin_memset(g_pwm_registers[driver], 0, sizeof(g_pwm_registers[0]));
    __builtin_memset(g_pwm_registers_need_update[driver], 0,
        sizeof(g_pwm_registers_need_update[0]));
}

// Initialize Function register (PG3) for other than the default values for its sub-
// function registers.
static void _is31_init_function_register(size_t driver)
{
    _is31_set_command_register(driver, SEL_PG3_FUNCTION);
    const uint8_t addr = DRIVER_ADDR[driver];

    // Set pull-up/down resistors for SWy and CSx.
    const uint8_t RESISTOR_8_KILOHM = 0x05;
    _is31_write_register(addr, PG3_SW_PULL_UP, RESISTOR_8_KILOHM);
    _is31_write_register(addr, PG3_CS_PULL_DOWN, RESISTOR_8_KILOHM);
}

void is31_init(void)
{
    // Turn off all LEDs and set PWM on all LEDs to 0
    sr_exp_writedata(0, SR_CTRL_IRST);
    ztimer_sleep(ZTIMER_USEC, 10);

    i2c_acquire(I2C);
    for ( size_t driver = 0 ; driver < DRIVER_COUNT ; driver++ ) {
        _is31_reset_registers(driver);
        _is31_init_function_register(driver);
    }
    i2c_release(I2C);

    // Release the Hardware Shutdown which has been asserted since booting. During
    // hardware shutdown state Function Register (PG3) can be operated. LEDs are still
    // turned off at this point, since all the Control registers (and the Global Current
    // Control register (GCR) as well) are reset to 0 above.
    sr_exp_writedata(SR_CTRL_SDB_N, 0);
    ztimer_sleep(ZTIMER_USEC, 10);

    // Wait 1 ms to ensure the device has woken up.
    // ztimer_sleep(ZTIMER_MSEC, 1);
}

void is31_switch_unlock_ssd(bool yes_no)
{
    i2c_acquire(I2C);
    for ( size_t driver = 0 ; driver < DRIVER_COUNT ; driver++ ) {
        _is31_set_command_register(driver, SEL_PG3_FUNCTION);
        _is31_write_register(DRIVER_ADDR[driver], PG3_CONFIGURATION,
            // Use sync = 1 (master clock mode) for DRIVER_ADDR[0], or 2 (clock slave
            // mode) otherwise.
            ((uint8_t)(driver == 0 ? 1 : 2) << 6) | yes_no);
    }
    i2c_release(I2C);
}

void is31_switch_led_on_off(is31_led_t led, bool on_off)
{
    const size_t driver = led.driver;
    const uint8_t reg_g = led.g / 8;
    const uint8_t reg_r = led.r / 8;
    const uint8_t reg_b = led.b / 8;
    const uint8_t bit_g = led.g % 8;
    const uint8_t bit_r = led.r % 8;
    const uint8_t bit_b = led.b % 8;

    uint8_t data_g;
    uint8_t data_r;
    uint8_t data_b;

    if ( on_off ) {
        data_g = (g_led_on_off_registers[driver][reg_g] |= ((uint8_t)1 << bit_g));
        data_r = (g_led_on_off_registers[driver][reg_r] |= ((uint8_t)1 << bit_r));
        data_b = (g_led_on_off_registers[driver][reg_b] |= ((uint8_t)1 << bit_b));
    } else {
        data_g = (g_led_on_off_registers[driver][reg_g] &= ~((uint8_t)1 << bit_g));
        data_r = (g_led_on_off_registers[driver][reg_r] &= ~((uint8_t)1 << bit_r));
        data_b = (g_led_on_off_registers[driver][reg_b] &= ~((uint8_t)1 << bit_b));
    }

    i2c_acquire(I2C);
    const uint8_t addr = DRIVER_ADDR[driver];
    _is31_set_command_register(driver, SEL_PG0_LED_CONTROL);
    _is31_write_register(addr, reg_g, data_g);
    _is31_write_register(addr, reg_r, data_r);
    _is31_write_register(addr, reg_b, data_b);
    i2c_release(I2C);
}

void is31_switch_all_leds_on_off(bool on_off)
{
    __builtin_memset(g_led_on_off_registers, (on_off ? 0xFF : 0),
        sizeof(g_led_on_off_registers));

    i2c_acquire(I2C);
    for ( size_t driver = 0 ; driver < DRIVER_COUNT ; driver++ )
        _is31_write_led_on_off_registers(driver);
    i2c_release(I2C);
}

static void _update_g_pwm_register(size_t driver, uint8_t reg, uint8_t data)
{
    if ( g_pwm_registers[driver][reg] != data ) {
        g_pwm_registers[driver][reg] = data;
        g_pwm_registers_need_update[driver][reg / 8] |= ((uint8_t)1 << (reg % 8));
    }
}

void is31_set_color(is31_led_t led, uint8_t red, uint8_t green, uint8_t blue)
{
    _update_g_pwm_register(led.driver, led.g, green);
    _update_g_pwm_register(led.driver, led.r, red);
    _update_g_pwm_register(led.driver, led.b, blue);
}

void is31_refresh_colors(void)
{
    i2c_acquire(I2C);
    for ( size_t driver = 0 ; driver < DRIVER_COUNT ; driver++ )
        _is31_write_pwm_registers(driver);
    i2c_release(I2C);
}

void is31_set_gcr(uint8_t gcr)
{
    i2c_acquire(I2C);
    for ( size_t driver = 0 ; driver < DRIVER_COUNT ; driver++ ) {
        _is31_set_command_register(driver, SEL_PG3_FUNCTION);
        _is31_write_register(DRIVER_ADDR[driver], PG3_GLOBAL_CURRENT, gcr);
    }
    i2c_release(I2C);
}
