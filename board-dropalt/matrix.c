#include "matrix.h"
#include "periph/gpio.h"        // also includes board-dropalt/include/periph_conf.h
#include "ztimer.h"             // for ztimer_spin()



static inline void select_col(unsigned col) { gpio_set(col_pins[col]); }

static inline void unselect_col(unsigned col) { gpio_clear(col_pins[col]); }

static inline void matrix_output_select_delay(void) { ztimer_spin(ZTIMER_USEC, 1); }

static debouncer_t _debouncer = NULL;

#if 1
// Optimized to leverage the fact that all row_pins belong to the same port group (e.g.
// PA), reducing matrix_scan() execution time, 30 µs vs 43 µs.

// Same definitions as in gpio.c
static inline PortGroup* _pin_port(gpio_t pin) {
    return (PortGroup*)(pin & ~(0x1fu));
}

static inline unsigned _pin_mask(gpio_t pin) {
    return 1u << (pin & 0x1fu);
}

static void read_rows_on_col(unsigned col)
{
    // Select col
    select_col(col);
    matrix_output_select_delay();  // gives a small delay after select.

    // Read the whole row at once.
    PortGroup* const port = _pin_port(row_pins[0]);
    const uint32_t in_reg = port->IN.reg;

    for ( unsigned row = 0 ; row < MATRIX_ROWS ; row++ )
        _debouncer(row * MATRIX_COLS + col, (in_reg & _pin_mask(row_pins[row])) != 0);

    // Unselect col
    unselect_col(col);
}
#else
static void read_rows_on_col(unsigned col)
{
    // Select col
    select_col(col);
    matrix_output_select_delay();  // gives a small delay after select.

    // For each row...
    for ( unsigned row = 0 ; row < MATRIX_ROWS ; row++ )
        _debouncer(row * MATRIX_COLS + col, gpio_read(row_pins[row]) != 0);

    // Unselect col
    unselect_col(col);
}
#endif

// GPIO pins for the matrix are configured with columns as outputs and rows as inputs,
// corresponding to DIODE_DIRECTION = ROW2COL in QMK. For COL2ROW setups, use
// read_cols_on_row() instead. Refer to quantum/matrix.c in QMK for implementation
// details.
void matrix_init(debouncer_t debouncer, gpio_cb_t isr, void* arg)
{
    _debouncer = debouncer;

    // Initialize GPIO pins
    for ( unsigned col = 0 ; col < MATRIX_COLS ; col++ ) {
        gpio_init(col_pins[col], GPIO_OUT);
        select_col(col);
    }

#if MODULE_PERIPH_GPIO_TAMPER_WAKE
#error MODULE_PERIPH_GPIO_TAMPER_WAKE is not compatible with custom use of gpio_init_int() for level-triggered GPIO_HIGH.
#endif

    // Use level-triggered (GPIO_HIGH = 0x4) interrupts instead of edge-triggered ones,
    // to ensure pin state changes during interrupt setup are not missed.
    static const unsigned GPIO_HIGH = 0x4;
    for ( unsigned row = 0 ; row < MATRIX_ROWS ; row++ )
        (void)gpio_init_int(row_pins[row], GPIO_IN_PD, GPIO_HIGH, isr, arg);
}

void matrix_enable_interrupt(void)
{
    for ( unsigned col = 0 ; col < MATRIX_COLS ; col++ )
        select_col(col);
    for ( unsigned row = 0 ; row < MATRIX_ROWS ; row++ )
        gpio_irq_enable(row_pins[row]);
}

void matrix_disable_interrupt(void)
{
    for ( unsigned row = 0 ; row < MATRIX_ROWS ; row++ )
        gpio_irq_disable(row_pins[row]);
    for ( unsigned col = 0 ; col < MATRIX_COLS ; col++ )
        unselect_col(col);
}

void matrix_scan(void)
{
    // Select each col and read rows.
    for ( unsigned col = 0 ; col < MATRIX_COLS ; col++ )
        read_rows_on_col(col);
}
