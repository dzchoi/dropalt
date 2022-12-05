#pragma once

#include <stdint.h>             // for uint8_t
#include <stdbool.h>            // for bool



#ifdef __cplusplus
extern "C" {
#endif

#define CS_LINE_COUNT               16  // CS 1-16: current source outputs
#define SW_LINE_COUNT               12  // SW 1-12: switch current inputs

#define PWM_REGISTER_COUNT          (CS_LINE_COUNT * SW_LINE_COUNT)
#define LED_CONTROL_REGISTER_COUNT  (PWM_REGISTER_COUNT / 8)

typedef struct {
    uint8_t driver;
    uint8_t g;
    uint8_t r;
    uint8_t b;
} is31_led_t;

extern char __STATIC_ASSERT__[1/( sizeof(is31_led_t) == 4 )];

// Initialize IS31FL3733 and release the Hardware Shutdown. Be aware that we further need
// to release SSD, set GCR, and turn on individual leds, to be able to see leds light.
void is31_init(void);

// By setting SSD bit of the PG3 Configuration Register to "0" (yes_no == false), the
// IS31FL3733 will operate in software shutdown mode, in which all current sources are
// switched off, so that the matrix is blanked. All registers can be operated.
void is31_switch_unlock_ssd(bool yes_no);

void is31_switch_led_on_off(is31_led_t, bool on_off);
void is31_switch_all_leds_on_off(bool on_off);

void is31_set_color(is31_led_t, uint8_t red, uint8_t green, uint8_t blue);
void is31_refresh_colors(void);

void is31_set_gcr(uint8_t gcr);

#ifdef __cplusplus
}
#endif
