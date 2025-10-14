#include "assert.h"

extern "C" {
#include "lua_run.h"            // for lua_riot_newstate()
#include "lualib.h"             // for luaopen_*()
}

#include "lua.hpp"
#include "lkeymap.hpp"          // for lua::load_keymap()



namespace lua {

static uint8_t lua_memory[LUA_MEM_SIZE]
    __attribute__((section(".noinit"), aligned(sizeof(uint32_t))));



global_lua_state::~global_lua_state()
{
    assert( lua_gettop(L) == 0 );
}

// Lua panic function invoked when an error occurs outside a protected environment.
static int _panic(lua_State* L)
{
    l_message(lua_tostring(L, -1));
    return 0;
}

// Exported from lfwlib.cpp
extern int luaopen_fw(lua_State* L);

void global_lua_state::init()
{
    L = lua_riot_newstate(lua_memory, sizeof(lua_memory), _panic);
    // assert( L != nullptr );
    global_lua_state L;  // Overrides global_lua_state::L.

    // Load some of the standard libraries.
    // Note: Avoid calling lua_riot_openlibs(), as it links all modules (used or not),
    // which unnecessarily increases the firmware image size.
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
    luaL_requiref(L, "fw", luaopen_fw, 0);  // It doesn't load into global environment.
    lua_settop(L, 0);  // Clear the lib addresses from the stack.

    // Load the "keymap" module into the registry.
    load_keymap();
}

}
