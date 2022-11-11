#pragma once

#include "map_modified.hpp"



namespace key {

enum mod_morph_flavor {
    KEEP_MOD = 0,  // default
    UNDO_MOD,
};

template <mod_morph_flavor>
class mod_morph_t;

// Deduction guide to be able to use bare mod_morph_t as mod_morph_t<KEEP_MOD>
mod_morph_t(map_t&, map_t&, map_t&) -> mod_morph_t<KEEP_MOD>;



// The 'keep-mod' flavor: The modifier is kept pressing when the modified version is
// registered.
template <>
class mod_morph_t<KEEP_MOD>: public map_modified_t {
public: // User-facing methods
    constexpr mod_morph_t(map_t& original, map_t& modified, map_t& modifier)
    : map_modified_t(modifier), m_original(original), m_modified(modified) {}

protected:
    map_t& m_original;
    map_t& m_modified;

private: // Methods to be called by map_modified_t
    void on_press(pmap_t* slot) { m_original.press(slot); }

    void on_release(pmap_t* slot) { m_original.release(slot); }

    void on_modified_press(pmap_t* slot) { m_modified.press(slot); }

    void on_modified_release(pmap_t* slot) { m_modified.release(slot); }
};



// The 'undo-mod' flavor: The modifier is released and pressed again while the modified
// version is registered.
template <>
class mod_morph_t<UNDO_MOD>: public mod_morph_t<KEEP_MOD> {
public:
    using mod_morph_t<KEEP_MOD>::mod_morph_t;

private:
    void on_modified_press(pmap_t* slot) {
        m_modifier.release(slot);
        m_modified.press(slot);
    }

    void on_modified_release(pmap_t* slot) {
        m_modified.release(slot);
        // Due to map_t::m_pressing_count being a signed integer this will not press the
        // modifier actually if it is already released physically.
        m_modifier.press(slot);
    }
};

}  // namespace key
