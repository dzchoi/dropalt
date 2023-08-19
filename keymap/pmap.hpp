#pragma once

#include <iterator>             // for std::input_iterator_tag
#include "color.hpp"            // for ohsv_t
#include "lamp.hpp"             // for lamp_t, lamp_iter
#include "led_conf.hpp"         // for LED_ID[], NUM_SLOTS



namespace key {

class map_t;
class pmap_t;



// The "matrix" that all keymaps reside in; `pmap_t*` can serve as a unique "slot id"
// that is assigned to each keymap in it.
// Note that the keymaps are not identified by some 8-bit or 16-bit keycodes like in QMK,
// but by their own lvalues. The literal_t keymaps do have their keycode() but more
// complex types do not.
extern pmap_t maps[NUM_SLOTS];

// Two-dimensional version of key::maps.
// inline pmap_t (&maps_2d)[MATRIX_ROWS][MATRIX_COLS] =
//     reinterpret_cast<pmap_t (&)[MATRIX_ROWS][MATRIX_COLS]>(maps);



class pmap_t {
public:
    // Constructor that assigns a slot in maps[] to the given keymap.
    constexpr pmap_t(map_t& map): m_pmap(&map) {}

    // Second constructor that also assigns a slot (led) to the given indicator lamp.
    // For example, defining `pmap_t { a_keymap, l_CAPSLOCK }` within `pmap_t maps[]`
    // would assign a slot to `a_keymap` and the slot's led to l_CAPSLOCK, where
    // l_CAPSLOCK can be defined as `lamp_t l_CAPSLOCK { CAPSLOCK_LAMP }`. Be aware that
    // `a_keymap` and l_CAPSLOCK do not interact with each other though they sit on the
    // same slot; `a_keymap` is driven by user's key events (presses and releases),
    // whereas l_CAPSLOCK is driven by the host's led_lamp_state report in response to
    // the corresponding key event (CAPSLOCK key in this case).
    pmap_t(map_t& map, lamp_t& lamp);

    map_t* operator->() { return m_pmap; }
    map_t& operator*() { return *m_pmap; }

    // `index()` assumes that instances of pmap_t be created only within maps[].
    size_t index() const { return this - &maps[0]; }

    // Return led_id corresponding to the slot.
    uint8_t led_id() const { return LED_ID[index()]; }

    // Return hsv if the slot's led is used for an indicator lamp and if the lamp is lit.
    // It is expensive as it walks through all the indicator lamps. Note that lamp_id()
    // is not provided from pmap_t, but from lamp_t.
    ohsv_t is_lamp_lit() const;

    // Not copyable nor movable. `pmap_t` will be instantiated within maps[] table.
    pmap_t(const pmap_t&) =delete;
    pmap_t& operator=(const pmap_t&) =delete;

private:
    map_t* const m_pmap;
};
static_assert( sizeof(pmap_t) == sizeof(map_t*) );

}  // namespace key



// Iterator over those slots that have
//  - valid led assigned and
//  - either no indicator lamp assigned or its indicator lamp turned off.
class led_iter {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = key::pmap_t;
    using pointer = value_type*;
    using reference = value_type&;

    led_iter(pointer slot): m_pslot(slot), m_plamp(nullptr) {}
    led_iter(pointer slot, lamp_iter plamp): m_pslot(slot), m_plamp(plamp) {}

    // Not copyable but movable.
    led_iter(led_iter&&) =default;
    led_iter(const led_iter&) =delete;
    led_iter& operator=(const led_iter&) =delete;

    reference operator*() const { return *m_pslot; }
    pointer operator->() const { return m_pslot; }

    // prefix increment
    led_iter& operator++() { ++m_pslot; return advance_if_invalid(); }

    friend bool operator==(const led_iter& a, const led_iter& b) {
        return a.m_pslot == b.m_pslot;
    }

    friend bool operator!=(const led_iter& a, const led_iter& b) {
        return !(a == b);
    }

    static led_iter begin();

    static led_iter end() { return led_iter(&key::maps[NUM_SLOTS]); }

private:
    pointer m_pslot;
    lamp_iter m_plamp;

    led_iter& advance_if_invalid();
};
