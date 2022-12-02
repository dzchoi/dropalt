#pragma once

#include "periph_conf.h"        // for MATRIX_ROWS and MATRIX_COLS
#include "led_conf.hpp"         // for led_conf.led_id[]



namespace key {

class map_t;
class pmap_t;
class pressing_slot;



// The "matrix" that all keymaps reside in; `pmap_t*` can serve as a unique "slot id"
// that is assigned to each keymap in it.
// Note that the keymaps are not identified by some 8-bit or 16-bit keycodes like in QMK,
// but by their own lvalues. The literal_t keymaps do have their keycode() but more
// complex types do not.
extern pmap_t maps[MATRIX_ROWS][MATRIX_COLS];



class pmap_t {
public:
    constexpr pmap_t(map_t& map): m_pmap(&map) {}
    map_t* operator->() { return m_pmap; }
    map_t& operator*() { return *m_pmap; }

    operator map_t*() { return m_pmap; }

    // Return the led_id associated with the slot.
    uint8_t led_id() const {
        size_t n = this - &maps[0][0];
        return led_conf.led_id[n / MATRIX_COLS][n % MATRIX_COLS];
    }

private:
    friend class pressing_slot;
    friend class manager_t;

    map_t* const m_pmap;
    pressing_slot* m_pressing = nullptr;
};

}  // namespace key
