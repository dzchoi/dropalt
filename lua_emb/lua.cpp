#include "assert.h"

extern "C" {
#include "lua_run.h"            // include "lua.h" besides lua_riot_newstate()
#include "lauxlib.h"            // declares luaL_*().
#include "lualib.h"             // luaopen_*()
}

#include "lua.hpp"
#include "lkeymap.hpp"          // for lua::load_keymap()



namespace lua {

static constexpr size_t LUA_MEM_SIZE = 100 *1024;

static uint8_t lua_memory[LUA_MEM_SIZE]
    __attribute__((section(".noinit"), aligned(sizeof(uint32_t))));

// Lua panic function invoked when an error occurs outside a protected environment.
static int _panic(lua_State* L)
{
    l_message(lua_tostring(L, -1));
    return 0;
}

// Exported from lfwlib.cpp
extern int luaopen_fw(lua_State* L);

void init()
{
    L = lua_riot_newstate(lua_memory, sizeof(lua_memory), _panic);
    assert( L != nullptr );

    // Load some of the standard libraries.
    // Note: Calling lua_riot_openlibs() would enlarge the firmware image by linking
    // all the modules even if we do not use some of them.
    luaL_requiref(L, "_G", luaopen_base, 1);  // Load into the global environment.
    // luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);   // 2K
    // luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);      // 3.3K
    // luaL_requiref(L, LUA_IOLIBNAME, luaopen_io, 1);          // 10.9K
    // luaL_requiref(L, LUA_OSLIBNAME, luaopen_os, 1);          // 18K
    // luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);     // 8.3K
    // luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);      // 1.4K
    // luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);      // 10.4K
    // luaL_requiref(L, LUA_DBLIBNAME, luaopen_debug, 1);       // 4.8K
    // Call luaopen_package() in contrib/lua_loadlib.c.
    luaL_requiref(L, LUA_LOADLIBNAME, luaopen_package, 1);      // 0.6K
    luaL_requiref(L, "fw", luaopen_fw, 0);  // Do not load into global environment.
    lua_settop(L, 0);  // Remove the libs on the stack.

    // Load the "keymap" module into the registry.
    load_keymap();
}

status_t ping::status() const
{
    if ( hd0 == '[' && hd1 == '}' && nl == '\n' )
        return st - '0';
    return LUA_NOSTATUS;
}

}
