#pragma once

#include "map_proxy.hpp"



namespace key {

template <class K>
// K can be either map_t& or a child class of map_t, as enforced by deduction guide.
class map_modified_t: public map_proxy_t {
protected: // Methods to be used by child classes
    // `K&&` here is a simple rvalue reference, rather than forwarding reference.
    // However, it can still represent an lvalue reference (using reference collapsing
    // rule) when K is set to the lvalue reference explicitly.
    constexpr map_modified_t(K&& modifier)
    : m_modifier(std::forward<K>(modifier)) {}

    K m_modifier;

private: // Methods to be called by key::manager
    void on_proxy_press(pmap_t* slot);

    void on_proxy_release(pmap_t* slot);

    virtual void on_modified_press(pmap_t* slot) =0;

    virtual void on_modified_release(pmap_t* slot) =0;

    bool m_is_modified = false;
};

template <class K>
// As a forwarding reference here `K&&` can represent both lvalue reference and rvalue
// reference. See https://stackoverflow.com/questions/60470661.
map_modified_t(K&&) -> map_modified_t<obj_or_ref_t<K>>;



template <class K>
void map_modified_t<K>::on_proxy_press(pmap_t* slot)
{
    assert( m_is_modified == false );
    if ( m_modifier.is_pressing() ) {
        m_is_modified = true;
        on_modified_press(slot);
    } else
        on_press(slot);
}

template <class K>
void map_modified_t<K>::on_proxy_release(pmap_t* slot)
{
    if ( m_is_modified ) {
        on_modified_release(slot);
        m_is_modified = false;
    } else
        on_release(slot);
}

}  // namespace key
