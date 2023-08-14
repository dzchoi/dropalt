#pragma once

#include <iterator>             // for std::input_iterator_tag
#include "color.hpp"            // for ohsv_t
#include "led_conf.hpp"         // for LED_ID[]
#include "lamp_id.hpp"          // for lamp_id_t
#include "periph_conf.h"        // for NUM_SLOTS



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
    constexpr pmap_t(map_t& map): m_pmap(&map) {}
    map_t* operator->() { return m_pmap; }
    map_t& operator*() { return *m_pmap; }

    operator map_t*() { return m_pmap; }

    // `index()` assumes that instances of pmap_t be created only in maps[].
    size_t index() const { return this - &maps[0]; }

    // Return led_id corresponding to the slot.
    uint8_t led_id() const { return LED_ID[index()]; }

    // Return lamp_id corresponding to the slot if its led is used for an indicator lamp.
    // Note that we could better isolate lamp_t from pmap_t by defining `lamp_t*` as an
    // additional data member in pmap_t and accessing lamp_id(), is_lamp_lit() and
    // refresh_lamp() through it. We would also need to change the constructor as
    // `constexpr pmap_t(map_t& map, lamp_t& lamp =no_lamp): m_pmap(&map), m_lamp(lamp)
    // {}`. However, it requires more space than the current implementation does.
    lamp_id_t lamp_id() const;

    // Return hsv if the slot's led is used for an indicator lamp and if its lamp state is
    // on.
    ohsv_t is_lamp_lit() const;

    // Refresh the lamp led according to the current state.
    void refresh_lamp();

    // Not copyable nor movable. `pmap_t` will be instantiated within `maps[]` table.
    pmap_t(const pmap_t&) =delete;
    pmap_t& operator=(const pmap_t&) =delete;

private:
    map_t* const m_pmap;
};

}  // namespace key



class slot_iter {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = key::pmap_t;
    using pointer = value_type*;
    using reference = value_type&;

    slot_iter(pointer slot): m_slot(slot) {}

    reference operator*() const { return *m_slot; }
    pointer operator->() const { return m_slot; }

    // prefix increment
    slot_iter& operator++() { ++m_slot; return *this; }

    static inline slot_iter begin() { return slot_iter(&key::maps[0]); }

    static inline slot_iter end() { return slot_iter(&key::maps[NUM_SLOTS]); }

    friend bool operator==(const slot_iter& a, const slot_iter& b) {
        return a.m_slot == b.m_slot;
    }

    friend bool operator!=(const slot_iter& a, const slot_iter& b) {
        return !(a == b);
    }

protected:
    pointer m_slot;
};

class led_iter: public slot_iter {
public:
    using slot_iter::slot_iter;

    led_iter& operator++() { ++m_slot; return advance_if_invalid(); }

    static inline led_iter begin() {
        return led_iter(&key::maps[0]).advance_if_invalid();
    }

    static inline led_iter end() { return led_iter(&key::maps[NUM_SLOTS]); }

private:
    led_iter& advance_if_invalid();
};

class lamp_iter: public slot_iter {
public:
    using slot_iter::slot_iter;

    // Note that instead of walking through all maps[] we could have a separate table
    // that lists only led_ids that are associated with an indicator lamp. But we assume
    // that lamp_iter is not used very often.
    lamp_iter& operator++() { ++m_slot; return advance_if_invalid(); }

    static inline lamp_iter begin() {
        return lamp_iter(&key::maps[0]).advance_if_invalid();
    }

    static inline lamp_iter end() { return lamp_iter(&key::maps[NUM_SLOTS]); }

private:
    lamp_iter& advance_if_invalid();
};
