extern "C" {
#include "lua_run.h"            // include "lua.h", which declares lua_*().
#include "lauxlib.h"            // declares luaL_*().
#include "lualib.h"             // luaopen_*()
}

#include "assert.h"

#include "lua.hpp"



namespace lua {

static constexpr size_t LUA_MEM_SIZE = 100 *1024;

static uint8_t lua_memory[LUA_MEM_SIZE] __attribute__((section(".noinit")));

// Lua keymap script and Lua Repl will share the same lua State.
static lua_State* L;



// Print all values of any type on the stack like Lua `print` (LuaB_print()). However,
// it prints each value on a separate line instead of joining them with '\t'.
static void lua_print(lua_State* L)
{
    for ( int i = -lua_gettop(L) ; i < 0 ; i++ ) {
        size_t l;
        const char* s = luaL_tolstring(L, i, &l);
        lua_writestring(s, l);
        lua_writeline();
        lua_pop(L, 1);
    }
    lua_settop(L, 0);  // Remove all values from the stack.

    // Alternatively, we could use "print" (i.e. luaB_print) from Lua base library.
    // int n = lua_gettop(L);
    // if ( n > 0 ) {  // Any result to print?
    //     // luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
    //     lua_getglobal(L, "print");
    //     lua_insert(L, 1);
    //     status = lua_pcall(L, n, 0, 0);
    // }
}



void init()
{
    L = lua_riot_newstate(lua_memory, sizeof(lua_memory), nullptr);
    assert( L != nullptr );

    lua_pushstring(L, LUA_COPYRIGHT);
    lua_print(L);

    // Load built-in modules under their module names.
    // Note: using lua_riot_openlibs() would increase the binary size by linking all
    // built-in modules even if we do not use some of them.
    luaL_requiref(L, "_G", luaopen_base, 1);  // Load into the global environment.
    // Call luaopen_package() from contrib/lua_loadlib.c. Do we need it?
    // luaL_requiref(L, LUA_LOADLIBNAME, luaopen_package, 1);  // 1.4K
    // luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);  // 2K
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    // luaL_requiref(L, LUA_IOLIBNAME, luaopen_io, 1);
    // luaL_requiref(L, LUA_OSLIBNAME, luaopen_os, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    // luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);  // 10K
    // luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    // luaL_requiref(L, LUA_DBLIBNAME, luaopen_debug, 1);
}

}
