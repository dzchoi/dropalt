#pragma once

#include <stdint.h>
#include "periph/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// Intialize the matrix.
void matrix_init(gpio_cb_t isr, void* arg);

// Enable interrupt-based scan mode.
void matrix_enable_interrupt(void);

// Enable active scan mode.
void matrix_disable_interrupt(void);

// Select `col` and return a bitmask where bit `row` is set iff that row is HIGH.
// Note: This can only be used after matrix_disable_interrupt() has been executed,
// ensuring that GPIO output select pins are unlocked for input selection.
uint32_t matrix_read_rows_on_col(unsigned col);

// Indices of matrix slots not physically connected to key switches or LEDs.
static const unsigned UNUSED_MATRIX_INDICES[] = { 42, 46, 63, 64, 65, 67, 68, 69 };

#ifdef __cplusplus
}
#endif
