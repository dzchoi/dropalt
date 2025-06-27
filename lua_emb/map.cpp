#include <new>                  // for placement new
#include "log.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include "lua.hpp"              // for lua::L
#include "map.hpp"
// #include "map_proxy.hpp"        // for map_proxy_t, on_proxy_press/release()



namespace lua {

namespace key {

// fw.map_pseudo() -> keymap
int map_t::_create(lua_State* L)
{
    void* memory = lua_newuserdata(L, sizeof(map_t));
    new (memory) map_t();
    luaL_setmetatable(L, "map_t");
    return 1;
}

map_t::~map_t()
{
    // While possible, creating a temporary keymap object at runtime is not desirable.
    LOG_WARNING("Map: ~map_t()\n");
}

void map_t::press()
{
    if ( m_press_count++ == 0 ) {
        // map_proxy_t* const pproxy = get_proxy();
        // if ( pproxy )
        //     pproxy->on_proxy_press();
        // else
            on_press();
    }
}

void map_t::release()
{
    if ( --m_press_count == 0 ) {
        // map_proxy_t* const pproxy = get_proxy();
        // if ( pproxy )
        //     pproxy->on_proxy_release();
        // else
            on_release();
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
