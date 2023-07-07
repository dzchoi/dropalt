#pragma once

#include "usb_thread.hpp"       // for hid_keyboard.get_lamp_state()



// Refer to https://wiki.osdev.org/USB_Human_Interface_Devices#LED_lamps for LED lamp
// bit positions in SetReport request from host.
enum lamp_id_t: uint8_t {
    NUMLOCK_LAMP    = 0,
    CAPSLOCK_LAMP   = 1,
    SCRLOCK_LAMP    = 2,
    COMPOSE_LAMP    = 3,
    KANA_LAMP       = 4,
    NUM_LAMPS,
    NO_LAMP         = 0xff,
};

static inline bool is_lamp_lit(lamp_id_t lamp_id) {
    return lamp_id < NUM_LAMPS
        && (usb_thread::obj().hid_keyboard.get_lamp_state() & (uint8_t(1) << lamp_id));
}
