#include <new>                  // for the placement new
#include "log.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include "hid_keycodes.hpp"     // for keycode()
#include "literal.hpp"



namespace lua {

namespace key {

int literal_t::_create(lua_State* L)
{
    const char* keyname = luaL_checkstring(L, 1);
    uint8_t keycode = ::keycode(keyname);
    if ( keycode == KC_NO )
        luaL_error(L, "invalid keyname \"%s\"", keyname);

    void* memory = lua_newuserdata(L, sizeof(literal_t));
    new (memory) literal_t(keycode);
    luaL_setmetatable(L, "map_t");
    return 1;
}

literal_t::~literal_t()
{
    LOG_WARNING("Map: ~literal_t()\n");
}

}

}
