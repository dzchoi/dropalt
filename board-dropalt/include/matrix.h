#pragma once

#include <stdint.h>
#include "periph/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#if (MATRIX_COLS <= 8)
typedef uint8_t matrix_row_t;
#elif (MATRIX_COLS <= 16)
typedef uint16_t matrix_row_t;
#elif (MATRIX_COLS <= 32)
typedef uint32_t matrix_row_t;
#else
#    error "MATRIX_COLS: invalid value"
#endif

// Intialize matrix for scaning.
void matrix_init(gpio_cb_t cb, void* arg);

void matrix_enable_interrupts(void);

void matrix_disable_interrupts(void);

// Scan all key states on matrix (without debouncing)
// Note that it can be used only after executing matrix_disable_interrupts(), so that
// gpio output select pins are unlocked to be able to select input pins.
bool matrix_scan(matrix_row_t raw_matrix[]);

#ifdef __cplusplus
}
#endif
