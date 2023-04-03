#pragma once

#include "map.hpp"

namespace key {

// if_t(cond, key1, key2) chooses key1 if cond evaluates to true, otherwise key2.

template <class K, class L>
class if_t: public map_t {
public: // User-facing methods
    constexpr if_t(bool (*cond)(), K&& key_true, L&& key_false)
    : m_cond(cond)
    , m_key_true(std::forward<K>(key_true))
    , m_key_false(std::forward<L>(key_false)) {}

private: // Methods to be called by key::manager
    bool (*const m_cond)();
    K m_key_true;
    L m_key_false;

    bool m_was_false = false;

    void on_press(pmap_t* slot);

    void on_release(pmap_t* slot);
};

template <class F, class K, class L>
if_t(F, K&&, L&&) -> if_t<obj_or_ref_t<K>, obj_or_ref_t<L>>;



template <class K, class L>
void if_t<K, L>::on_press(pmap_t* slot)
{
    assert( m_was_false == false );
    if ( m_cond() )
        m_key_true.press(slot);
    else {
        m_was_false = true;
        m_key_false.press(slot);
    }
}

template <class K, class L>
void if_t<K, L>::on_release(pmap_t* slot)
{
    if ( m_was_false ) {
        m_key_false.release(slot);
        m_was_false = false;
    } else
        m_key_true.release(slot);
}

}  // namespace key
