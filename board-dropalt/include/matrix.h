#pragma once

#include "periph/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*debouncer_t)(unsigned slot_index, bool pressing);

// Intialize the matrix.
void matrix_init(debouncer_t debouncer, gpio_cb_t isr, void* arg);

// Enable interrupt-based scan mode.
void matrix_enable_interrupt(void);

// Enable active scan mode.
void matrix_disable_interrupt(void);

// Scan all keys on the matrix.
// Note: This can only be used after matrix_disable_interrupt() has been executed,
// ensuring that GPIO output select pins are unlocked for input selection.
void matrix_scan(void);

#ifdef __cplusplus
}
#endif
