#include "board.h"              // for DRIVER_COUNT, DRIVER_ADDR, ...
#include "is31fl3733.h"
#include "periph/i2c.h"         // for i2c_write_bytes(), i2c_read_bytes(), ...
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
uint8_t g_pwm_registers_need_update[DRIVER_COUNT][PWM_REGISTER_COUNT / 8]
    __attribute__((aligned(sizeof(uint32_t))));

static_assert( PWM_REGISTER_COUNT % 8 == 0 );
static_assert( sizeof(g_pwm_registers_need_update) % sizeof(uint32_t) == 0 );



// If transfer fails function returns false.
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

static void _is31_set_command_register(unsigned driver, uint8_t page)
{
    if ( g_command_register[driver] != page ) {
        const uint8_t addr = DRIVER_ADDR[driver];
        _is31_write_register(addr, REG_COMMAND_WRITE_LOCK, SEL_UNLOCK);
        _is31_write_register(addr, REG_COMMAND, page);
        g_command_register[driver] = page;
    }
}

static void _is31_write_led_on_off_registers(unsigned driver)
{
    _is31_set_command_register(driver, SEL_PG0_LED_CONTROL);
    _is31_write_registers(DRIVER_ADDR[driver],
        0, g_led_on_off_registers[driver], sizeof(g_led_on_off_registers[0]));
}

static void _is31_write_pwm_registers(unsigned driver)
{
#ifndef MODULE_PERIPH_DMA
#   error _is31_write_pwm_registers() is implemented only with MODULE_PERIPH_DMA enabled.
#endif
    _is31_set_command_register(driver, SEL_PG1_PWM);

    const uint8_t addr = DRIVER_ADDR[driver];
    unsigned reg_start = 0;       // Starting register index for batch write
    unsigned reg_count = 0;       // Number of consecutive pwm registers to update
    bool needs_new_block = true;  // Indicates if a new block should begin

    // Process 32-bit flag blocks.
    for ( unsigned i = 0 ; i < sizeof(g_pwm_registers_need_update[0]) ; i += 4 ) {
        uint32_t flags = *(uint32_t*)(void*)&g_pwm_registers_need_update[driver][i];
        *(uint32_t*)(void*)&g_pwm_registers_need_update[driver][i] = 0;

        unsigned bits = 0;  // Number of bits processed in this chunk
        while ( flags ) {
            // Skip zeros.
            const unsigned zeros = __builtin_ctz(flags);
            flags >>= zeros;
            bits += zeros;

            if ( zeros > 0 || needs_new_block ) {
                // Start of a new block — write the previous block first.
                if ( reg_count > 0 ) {
                    _is31_write_registers(addr,
                        reg_start, &g_pwm_registers[driver][reg_start], reg_count);
                    reg_count = 0;
                }
                reg_start = i * 8 + bits;
            }

            // Count consecutive '1's.
            const unsigned ones = unlikely(flags == ~0u) ? 32 : __builtin_ctz(~flags);
            flags >>= ones;
            bits += ones;
            reg_count += ones;
        }

        // If we exited early (not all 32 bits were processed), a new block is needed
        // next.
        needs_new_block = (bits < 32);
    }

    // Handle any remaining unwritten block at the end.
    if ( reg_count > 0 )
        _is31_write_registers(addr,
            reg_start, &g_pwm_registers[driver][reg_start], reg_count);
}

// Read reset register to intialize all registers to their default values, so Control
// registers and PWM registers will be set to all 0.
static void _is31_reset_registers(unsigned driver)
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

// Configure Function register (PG3), overriding default settings of sub-function
// registers.
static void _is31_init_function_register(unsigned driver)
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
    // Turn off all LEDs and set PWM for all LEDs to 0
    sr_exp_writedata(0, SR_CTRL_IRST);
    ztimer_sleep(ZTIMER_USEC, 10);

    i2c_acquire(I2C);
    for ( unsigned driver = 0 ; driver < DRIVER_COUNT ; driver++ ) {
        _is31_reset_registers(driver);
        _is31_init_function_register(driver);
    }
    i2c_release(I2C);

    // Release the Hardware Shutdown which has been asserted since booting. During
    // Hardware Shutdown state, Function Register (PG3) can be operated. LEDs are still
    // turned off at this point, since all Control registers (and Global Current Control
    // register (GCR) as well) are reset to 0 above.
    sr_exp_writedata(SR_CTRL_SDB_N, 0);
    ztimer_sleep(ZTIMER_USEC, 10);

    // Wait 1 ms to ensure the device has woken up.
    // ztimer_sleep(ZTIMER_MSEC, 1);
}

void is31_set_ssd_lock(bool enable)
{
    const bool ssd_bit = !enable;
    i2c_acquire(I2C);
    for ( unsigned driver = 0 ; driver < DRIVER_COUNT ; driver++ ) {
        _is31_set_command_register(driver, SEL_PG3_FUNCTION);

        // If the SSD bit in the PG3 Configuration Register is set to 0, the IS31FL3733
        // enters Software Shutdown mode. Set the SSD bit to 1 for normal operation.
        _is31_write_register(DRIVER_ADDR[driver], PG3_CONFIGURATION,
            // Use sync = 1 (master clock mode) for DRIVER_ADDR[0], or 2 (clock slave
            // mode) otherwise.
            ((uint8_t)(1 + (driver != 0)) << 6) | ssd_bit);
    }
    i2c_release(I2C);
}

void is31_enable_led(is31_led_t led, bool enable)
{
    const unsigned driver = led.driver;
    const uint8_t addr = DRIVER_ADDR[driver];

    i2c_acquire(I2C);
    _is31_set_command_register(driver, SEL_PG0_LED_CONTROL);

    uint8_t reg = led.reg_g;
    for ( int i = 0 ; i < 3 ; i++, reg += CS_LINE_COUNT ) {  // Traverse reg_g/r/b.
        const uint8_t row = reg / 8;
        const uint8_t col = reg % 8;
        const uint8_t on_off = enable
            ? (g_led_on_off_registers[driver][row] |= (1 << col))
            : (g_led_on_off_registers[driver][row] &= ~(1 << col));
        _is31_write_register(addr, row, on_off);
    }

    i2c_release(I2C);
}

void is31_enable_all_leds(bool enable)
{
    static_assert( -true == ~0 );
    __builtin_memset(g_led_on_off_registers, -enable, sizeof(g_led_on_off_registers));

    i2c_acquire(I2C);
    for ( unsigned driver = 0 ; driver < DRIVER_COUNT ; driver++ )
        _is31_write_led_on_off_registers(driver);
    i2c_release(I2C);
}

static void _update_g_pwm_register(unsigned driver, uint8_t reg, uint8_t data)
{
    if ( g_pwm_registers[driver][reg] != data ) {
        g_pwm_registers[driver][reg] = data;
        g_pwm_registers_need_update[driver][reg / 8] |= ((uint8_t)1 << (reg % 8));
    }
}

void is31_set_color(is31_led_t led, uint8_t red, uint8_t green, uint8_t blue)
{
    const uint8_t driver = led.driver;
    const uint8_t reg_g = led.reg_g;
    const uint8_t reg_r = reg_g + CS_LINE_COUNT;
    const uint8_t reg_b = reg_r + CS_LINE_COUNT;

    _update_g_pwm_register(driver, reg_g, green);
    _update_g_pwm_register(driver, reg_r, red);
    _update_g_pwm_register(driver, reg_b, blue);
}

void is31_refresh_colors(void)
{
    i2c_acquire(I2C);
    for ( unsigned driver = 0 ; driver < DRIVER_COUNT ; driver++ )
        _is31_write_pwm_registers(driver);
    i2c_release(I2C);
}

void is31_set_gcr(uint8_t gcr)
{
    i2c_acquire(I2C);
    for ( unsigned driver = 0 ; driver < DRIVER_COUNT ; driver++ ) {
        _is31_set_command_register(driver, SEL_PG3_FUNCTION);
        _is31_write_register(DRIVER_ADDR[driver], PG3_GLOBAL_CURRENT, gcr);
    }
    i2c_release(I2C);
}
