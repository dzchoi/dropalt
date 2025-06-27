#include <new>                  // for the placement new
#include "log.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include "if.hpp"
#include "lua.hpp"              // for lua::L



namespace lua {

namespace key {

using lua::L;

int if_t::_create(lua_State* L)
{
    // ( pred keymap1 keymap2 -- )
    luaL_checktype(L, 1, LUA_TFUNCTION);
    luaL_checkudata(L, 2, "map_t");
    luaL_checkudata(L, 3, "map_t");

    int ref_keymap2 = luaL_ref(L, LUA_REGISTRYINDEX);
    int ref_keymap1 = luaL_ref(L, LUA_REGISTRYINDEX);
    int ref_pred = luaL_ref(L, LUA_REGISTRYINDEX);

    void* memory = lua_newuserdata(L, sizeof(if_t));
    new (memory) if_t(ref_pred, ref_keymap1, ref_keymap2);
    luaL_setmetatable(L, "map_t");
    // ( -- userdata )
    return 1;
}

if_t::~if_t()
{
    LOG_WARNING("Map: ~if_t()\n");
    luaL_unref(L, LUA_REGISTRYINDEX, m_pred);
    luaL_unref(L, LUA_REGISTRYINDEX, m_keymap1);
    luaL_unref(L, LUA_REGISTRYINDEX, m_keymap2);
}

void if_t::on_press()
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_pred);
    lua_call(L, 0, 1);  // Non-protected call
    // ( -- cond )
    bool cond = lua_toboolean(L, -1);
    lua_remove(L, 1);

    if ( cond )
        deref(m_keymap1)->press();
    else {
        m_was_false = true;
        deref(m_keymap2)->press();
    }
}

void if_t::on_release()
{
    if ( m_was_false ) {
        deref(m_keymap2)->release();
        m_was_false = false;
    }
    else
        deref(m_keymap1)->release();
}

}

}
