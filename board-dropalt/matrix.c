#include "matrix.h"
#include "periph/gpio.h"
#include "xtimer.h"             // for xtimer_spin()



static inline void select_col(unsigned col) { gpio_set(col_pins[col]); }

static inline void unselect_col(unsigned col) { gpio_clear(col_pins[col]); }

static inline void matrix_output_select_delay(void) { xtimer_spin(1); }


// Note that the gpio pins for matrix are assumed to be output for Col and input for Row,
// that is, DIODE_DIRECTION == ROW2COL in terms of QMK. For COL2ROW instead, we should use
// read_cols_on_row(). See quantum/matrix.c in QMK.

#if 1
// This works only when all row_pins have the same port group (e.g. PA), but matrix_scan()
// takes less time, 30 us vs 43 us.

// Same definitions as in gpio.c
static inline PortGroup* _pin_port(gpio_t pin) {
    return (PortGroup*)(pin & ~(0x1fu));
}

static inline unsigned _pin_mask(gpio_t pin) {
    return 1u << (pin & 0x1fu);
}

static inline bool read_rows_on_col(matrix_row_t raw_matrix[], unsigned col)
{
    bool changed = false;

    // Select col
    select_col(col);
    matrix_output_select_delay();  // gives a small delay after select.

    // Read the whole row at once.
    // Todo: Put these in critical section?
    PortGroup* const port = _pin_port(row_pins[0]);
    const uint32_t in_reg = port->IN.reg;

    for ( unsigned row_index = 0 ; row_index < MATRIX_ROWS ; row_index++ ) {
        const matrix_row_t last_row_value = raw_matrix[row_index];
        if ( (in_reg & _pin_mask(row_pins[row_index])) != 0u )
            raw_matrix[row_index] |= ((matrix_row_t)1 << col);
        else
            raw_matrix[row_index] &= ~((matrix_row_t)1 << col);
        if ( last_row_value != raw_matrix[row_index] )
            changed = true;
    }

    // Unselect col
    unselect_col(col);

    return changed;
}
#else
static inline bool read_rows_on_col(matrix_row_t raw_matrix[], unsigned col)
{
    bool changed = false;

    // Select col
    select_col(col);
    matrix_output_select_delay();  // gives a small delay after select.

    // For each row...
    for ( unsigned row_index = 0 ; row_index < MATRIX_ROWS ; row_index++ ) {
        const matrix_row_t last_row_value = raw_matrix[row_index];
        if ( gpio_read(row_pins[row_index]) != 0 )
            // Pin HI, set col bit
            raw_matrix[row_index] |= ((matrix_row_t)1 << col);
        else
            // Pin LO, clear col bit
            raw_matrix[row_index] &= ~((matrix_row_t)1 << col);
        if ( last_row_value != raw_matrix[row_index] )
            changed = true;
    }

    // Unselect col
    unselect_col(col);

    return changed;
}
#endif

void matrix_init(gpio_cb_t cb, void* arg)
{
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

bool matrix_scan(matrix_row_t raw_matrix[])
{
    bool changed = false;

    // Set col and read rows.
    for ( unsigned col = 0 ; col < MATRIX_COLS ; col++ )
        if ( read_rows_on_col(raw_matrix, col) )
            changed = true;

    return changed;
}