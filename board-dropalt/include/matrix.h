#pragma once

#include "periph/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*debouncer_t)(unsigned row, unsigned col, bool is_press);

// Intialize matrix for scaning.
void matrix_init(debouncer_t debouncer, gpio_cb_t cb, void* arg);

void matrix_enable_interrupts(void);

void matrix_disable_interrupts(void);

// Scan all key states on matrix.
// Note that it can be used only after executing matrix_disable_interrupts(), so that
// gpio output select pins are unlocked to be able to select input pins.
void matrix_scan(void);

#ifdef __cplusplus
}
#endif
