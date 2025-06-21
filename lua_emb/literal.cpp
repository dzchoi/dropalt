#include <new>                  // for the placement new

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include "literal.hpp"



namespace lua {

namespace key {

int literal_t::_create(lua_State* L)
{
    uint8_t keycode = luaL_checkinteger(L, -1);
    void* memory = lua_newuserdata(L, sizeof(literal_t));
    new (memory) literal_t(keycode);
    luaL_setmetatable(L, "literal_t");
    return 1;
}

int literal_t::__gc(lua_State* L)
{
    literal_t* obj = static_cast<literal_t*>(lua_touserdata(L, -1));
    obj->~literal_t();
    return 0;
}

}

}
