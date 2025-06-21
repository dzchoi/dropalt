#include <new>                  // for placement new

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include "lua.hpp"              // for lua::L
#include "map.hpp"
// #include "map_proxy.hpp"        // for map_proxy_t, on_proxy_press/release()



namespace lua {

namespace key {

int map_t::_create(lua_State* L)
{
    void* memory = lua_newuserdata(L, sizeof(map_t));
    new (memory) map_t();
    luaL_setmetatable(L, "map_t");
    return 1;
}

int map_t::__gc(lua_State* L)
{
    map_t* obj = static_cast<map_t*>(lua_touserdata(L, -1));
    obj->~map_t();  // Any Lua references should be deleted by ~map_t().
    return 0;
}

void map_t::press(unsigned slot_index)
{
    if ( m_press_count++ == 0 ) {
        // map_proxy_t* const pproxy = get_proxy();
        // if ( pproxy )
        //     pproxy->on_proxy_press(slot_index);
        // else
            on_press(slot_index);
    }
}

void map_t::release(unsigned slot_index)
{
    if ( --m_press_count == 0 ) {
        // map_proxy_t* const pproxy = get_proxy();
        // if ( pproxy )
        //     pproxy->on_proxy_release(slot_index);
        // else
            on_release(slot_index);
    }
}

map_t* map_t::deref(int ref)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    map_t* obj = static_cast<map_t*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return obj;
}

}

}
