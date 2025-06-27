#pragma once

#include "map.hpp"



namespace lua {

namespace key {

class if_t: public map_t {
public: // User-facing methods callable from Lua
    // fw.map_if(pred, keymap1, keymap2) -> keymap
    // acts as keymap1 if pred() returns true, otherwise acts as keymap2. Here, pred is
    // a function object with the type: pred() -> bool.
    static int _create(lua_State* L);

    void _delete() override { this->~if_t(); }

protected:
    explicit if_t(int ref_pred, int ref_keymap1, int ref_keymap2)
    : m_pred(ref_pred), m_keymap1(ref_keymap1), m_keymap2(ref_keymap2) {}

    ~if_t();

private: // Methods accessed via the base class map_t
    void on_press() override;
    void on_release() override;

    int m_pred;
    int m_keymap1;
    int m_keymap2;

    bool m_was_false = false;
};

}

}
