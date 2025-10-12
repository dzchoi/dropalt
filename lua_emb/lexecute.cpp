#include "log.h"

#include "lexecute.hpp"
#include "lua.hpp"



namespace lua {

int execute_later(lua_State* L)
{
    LOG_DEBUG("Lua: execute_later()");
    luaL_checktype(L, 1, LUA_TFUNCTION);
    int n = lua_gettop(L);
    lua_newtable(L);
    lua_insert(L, 1);
    // ( -- call_frame f arg1 ... )
    for ( int i = n ; i > 0 ; i-- )  // Pack!
        lua_rawseti(L, 1, i);
    // ( -- call_frame )

    lua_pushlightuserdata(L, (void*)&execute_later);
    lua_pushvalue(L, -1);
    lua_insert(L, 1);
    // ( -- &execute_later call_fram &execute_later )
    lua_gettable(L, LUA_REGISTRYINDEX);
    // ( -- &execute_later call_frame next_call_frame )
    lua_rawseti(L, 2, 0);  // call_frame[0] = next_call_frame
    // ( -- &execute_later call_frame )
    lua_settable(L, LUA_REGISTRYINDEX);
    // ( -- )
    return 0;
}

bool execute_is_pending()
{
    global_lua_state L;

    lua_pushlightuserdata(L, (void*)&execute_later);
    lua_gettable(L, LUA_REGISTRYINDEX);
    // ( -- next_call_frame )
    bool result = lua_isnil(L, -1);
    lua_pop(L, 1);
    // ( -- )
    return !result;
}

void execute_pending_calls()
{
    global_lua_state L;
    LOG_DEBUG("Lua: execute_pending_calls()");

    lua_pushlightuserdata(L, (void*)&execute_later);
    lua_pushvalue(L, -1);
    lua_gettable(L, LUA_REGISTRYINDEX);
    // ( -- &execute_later call_frame )

    const int call_frame = lua_absindex(L, -1);
    while ( !lua_isnil(L, -1) ) {
        // ( -- &execute_later call_frame )
        int n = lua_rawlen(L, -1);  // Counts from call_frame[1], not call_frame[0].
        for ( int i = 1 ; i <= n ; i++ )  // Unpack!
            lua_rawgeti(L, call_frame, i);
        // ( -- &execute_later call_frame f arg1 ... )
        int status = lua_pcall(L, n - 1, 0, 0);
        // ( -- &execute_later call_frame [error_msg] )
        if ( status != LUA_OK ) {
            l_message(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        // ( -- &execute_later call_frame )

        // Clean up each call frame.
        lua_rawgeti(L, -1, 0);
        lua_insert(L, -2);
        // ( -- &execute_later next_call_frame call_frame )
        lua_pushnil(L);
        // ( -- &execute_later next_call_frame call_frame nil )
        lua_rawseti(L, -2, 0);  // call_frame[0] = nil
        // ( -- &execute_later next_call_frame call_frame )
        lua_pop(L, 1);
        // ( -- &execute_later next_call_frame )
    }

    // Clean up the pending call list.
    // ( -- &execute_later nil )
    lua_settable(L, LUA_REGISTRYINDEX);
    // ( -- )
}

}
