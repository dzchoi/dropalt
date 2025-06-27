#pragma once

#include "map.hpp"



namespace lua {

namespace key {

class literal_t: public map_t {
public: // User-facing methods callable from Lua
    // fw.map_literal(keyname: string) -> keymap
    // creates a keymap object that presses or releases the specified key. Refer to
    // keycode_to_name[] for available key names.
    // Note: Each map_t instance is independent and tracks its own press state. Although
    // calling map_literal() multiple times with the same key name is allowed, it is
    // generally discouraged. If you want multiple key slots to behave identically (e.g.
    // left and right FN), create a single instance and assign it to both slots.
    static int _create(lua_State* L);

    void _delete() override { this->~literal_t(); }

protected:
    explicit literal_t(uint8_t keycode): m_code(keycode) {}

    ~literal_t();

    uint8_t keycode() const { return m_code; }

private: // Methods accessed via the base class map_t
    void on_press() override { send_press(m_code); }
    void on_release() override { send_release(m_code); }

    const uint8_t m_code;
};

}

}
