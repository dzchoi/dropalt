#include "matrix.h"
#include "periph/gpio.h"
#include "ztimer.h"             // for ztimer_spin()



static inline void select_col(unsigned col) { gpio_set(col_pins[col]); }

static inline void unselect_col(unsigned col) { gpio_clear(col_pins[col]); }

static inline void matrix_output_select_delay(void) { ztimer_spin(ZTIMER_USEC, 1); }

static debouncer_t _debouncer = NULL;

// Note that the gpio pins for matrix are assumed to be output for Col and input for Row,
// that is, DIODE_DIRECTION == ROW2COL in terms of QMK. For COL2ROW instead, we should use
// read_cols_on_row(). See quantum/matrix.c in QMK.

#if 1
// This works only if all row_pins have the same port group (e.g. PA), but matrix_scan()
// will take less time, 30 us vs 43 us.

// Same definitions as in gpio.c
static inline PortGroup* _pin_port(gpio_t pin) {
    return (PortGroup*)(pin & ~(0x1fu));
}

static inline unsigned _pin_mask(gpio_t pin) {
    return 1u << (pin & 0x1fu);
}

static inline void read_rows_on_col(unsigned col)
{
    // Select col
    select_col(col);
    matrix_output_select_delay();  // gives a small delay after select.

    // Read the whole row at once.
    PortGroup* const port = _pin_port(row_pins[0]);
    const uint32_t in_reg = port->IN.reg;

    for ( unsigned row = 0 ; row < MATRIX_ROWS ; row++ )
        _debouncer(row, col, (in_reg & _pin_mask(row_pins[row])) != 0u);

    // Unselect col
    unselect_col(col);
}
#else
static inline void read_rows_on_col(unsigned col)
{
    // Select col
    select_col(col);
    matrix_output_select_delay();  // gives a small delay after select.

    // For each row...
    for ( unsigned row = 0 ; row < MATRIX_ROWS ; row++ )
        _debouncer(row, col, gpio_read(row_pins[row]) != 0);

    // Unselect col
    unselect_col(col);
}
#endif

void matrix_init(debouncer_t debouncer, gpio_cb_t cb, void* arg)
{
    _debouncer = debouncer;

    // initialize gpio pins
    for ( unsigned col = 0 ; col < MATRIX_COLS ; col++ ) {
        gpio_init(col_pins[col], GPIO_OUT);
        select_col(col);
    }

    for ( unsigned row = 0 ; row < MATRIX_ROWS ; row++ )
        (void)gpio_init_int(row_pins[row], GPIO_IN_PD, GPIO_RISING, cb, arg);
}

void matrix_enable_interrupts(void)
{
    for ( unsigned col = 0 ; col < MATRIX_COLS ; col++ )
        select_col(col);
    for ( unsigned row = 0 ; row < MATRIX_ROWS ; row++ )
        gpio_irq_enable(row_pins[row]);
}

void matrix_disable_interrupts(void)
{
    for ( unsigned row = 0 ; row < MATRIX_ROWS ; row++ )
        gpio_irq_disable(row_pins[row]);
    for ( unsigned col = 0 ; col < MATRIX_COLS ; col++ )
        unselect_col(col);
}

void matrix_scan(void)
{
    // Set col and read rows.
    for ( unsigned col = 0 ; col < MATRIX_COLS ; col++ )
        read_rows_on_col(col);
}
