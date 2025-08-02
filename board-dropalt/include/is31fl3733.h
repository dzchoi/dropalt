#pragma once

#include <stdbool.h>            // for bool

#include "led_conf.h"           // for is31_led_t, ...



#ifdef __cplusplus
extern "C" {
#endif

#define PWM_REGISTER_COUNT          (CS_LINE_COUNT * SW_LINE_COUNT)
#define LED_CONTROL_REGISTER_COUNT  (PWM_REGISTER_COUNT / 8)

static_assert( SW_CS_TO_REGISTER_END == PWM_REGISTER_COUNT );

// Initialize IS31FL3733 and release the Hardware Shutdown. Note that we further need
// to release SSD, set GCR, and turn on individual leds, to be able to see leds light.
void is31_init(void);

// Control Software Shutdown mode on the IS31FL3733.
// When enabled, all current sources are turned off - the LED matrix appears blanked,
// but registers remain accessible.
void is31_set_ssd_lock(bool enable);

void is31_enable_led(is31_led_t, bool enable);
void is31_enable_all_leds(bool enable);

void is31_set_color(is31_led_t, uint8_t red, uint8_t green, uint8_t blue);
void is31_refresh_colors(void);

// Set GCR (0–255) to control overall LED brightness.
void is31_set_gcr(uint8_t gcr);

#ifdef __cplusplus
}
#endif
