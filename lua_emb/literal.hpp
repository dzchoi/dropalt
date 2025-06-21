#pragma once

#include "map.hpp"



namespace lua {

namespace key {

class literal_t: public map_t {
public: // User-facing methods callable from Lua
    // ( keycode -- instance )
    static int _create(lua_State* L);

    // ( instance -- )
    static int __gc(lua_State* L);

protected:
    explicit literal_t(uint8_t keycode): m_code(keycode) {}

    ~literal_t() =default;

    uint8_t keycode() const { return m_code; }

private: // Methods accessed via the base class map_t
    void on_press(unsigned) override { send_press(m_code); }
    void on_release(unsigned) override { send_release(m_code); }

    const uint8_t m_code;
};

}

}
