#pragma once

// [OBSOLETE] mod_morph_t and map_modified_t can be replaced with if_t.
// mod_morph_t(key1, key2, key3) == if_t( [](){ return key3.is_pressed(); }, key2, key1 )

#include "map_modified.hpp"



namespace key {

// Flavors of mod_morph_t
constexpr struct keep_mod_t {} KEEP_MOD;  // default
constexpr struct undo_mod_t {} UNDO_MOD;

template <class, class, class, class>
class mod_morph_t;

// Deduction guide to be able to use KEEP_MOD by default.
template <class K, class L, class M>
mod_morph_t(K&&, L&&, M&&) ->
    mod_morph_t<keep_mod_t, obj_or_ref_t<K>, obj_or_ref_t<L>, obj_or_ref_t<M>>;

template <class F, class K, class L, class M>
mod_morph_t(K&&, L&&, M&&, F) ->
    mod_morph_t<F, obj_or_ref_t<K>, obj_or_ref_t<L>, obj_or_ref_t<M>>;



// The 'keep-mod' flavor: The modifier is kept pressing when the modified version is
// registered.
template <class K, class L, class M>
class mod_morph_t<keep_mod_t, K, L, M>: public map_modified_t<M> {
public: // User-facing methods
    constexpr mod_morph_t(K&& original, L&& modified, M&& modifier, keep_mod_t =KEEP_MOD)
    : map_modified_t<M>(std::forward<M>(modifier))
    , m_original(std::forward<K>(original))
    , m_modified(std::forward<L>(modified)) {}

protected:
    K m_original;
    L m_modified;

private: // Methods to be called by map_modified_t
    void on_press(pmap_t* slot) { m_original.press(slot); }

    void on_release(pmap_t* slot) { m_original.release(slot); }

    void on_modified_press(pmap_t* slot) { m_modified.press(slot); }

    void on_modified_release(pmap_t* slot) { m_modified.release(slot); }
};



// The 'undo-mod' flavor: The modifier is released and pressed again while the modified
// version is registered.
template <class K, class L, class M>
class mod_morph_t<undo_mod_t, K, L, M>: public mod_morph_t<keep_mod_t, K, L, M> {
public:
    constexpr mod_morph_t(K&& original, L&& modified, M&& modifier, undo_mod_t =UNDO_MOD)
    : mod_morph_t<keep_mod_t, K, L, M>::mod_morph_t(
        std::forward<K>(original), std::forward<L>(modified), std::forward<M>(modifier))
    {}

private:
    using mod_morph_t<keep_mod_t, K, L, M>::m_modified;
    using map_modified_t<M>::m_modifier;

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
