#pragma once

namespace key {

class map_t;
class pressing_slot;



class pmap_t {
public:
    constexpr pmap_t(map_t& map): m_pmap(&map) {}
    map_t* operator->() { return m_pmap; }
    map_t& operator*() { return *m_pmap; }

    operator map_t*() { return m_pmap; }

    // Todo: The pressing slot list can help determining whether to report to maps[][] or
    // not, also helps eager debouncing per key.
    pressing_slot* m_pressing = nullptr;

private:
    // friend class pressing_slot;
    // friend class whole_t;
    map_t* const m_pmap;
};



// The "matrix" that all keymaps reside in; `pmap_t*` can serve as a unique "slot id"
// that is assigned to each keymap in it.
// Note that the keymaps are not identified by some 8-bit or 16-bit keycodes like in QMK,
// but by their own lvalues. The literal_t keymaps do have their keycode() but more
// complex types do not.
extern pmap_t maps[MATRIX_ROWS][MATRIX_COLS];

}  // namespace key
