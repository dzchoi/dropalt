#include <stdio.h>              // for fputs()

#include "assert.h"
#include "compiler_hints.h"     // for unlikely()

extern "C" {
#include "lua_run.h"            // include "lua.h" besides lua_riot_newstate()
#include "lauxlib.h"            // declares luaL_*().
#include "lualib.h"             // luaopen_*()
}

#include "lua.hpp"
#include "riotboot/slot.h"      // for riotboot_slot_validate(), ...



namespace lua {

static constexpr size_t LUA_MEM_SIZE = 100 *1024;

static uint8_t lua_memory[LUA_MEM_SIZE] __attribute__((section(".noinit")));

// Lua keymap script and Lua Repl will share the same lua State.
static lua_State* L;



// Lua panic function invoked when an error occurs outside a protected environment.
static int _panic(lua_State* L)
{
    l_message(lua_tostring(L, -1));
    return 0;
}

constexpr unsigned SLOT1 = 1;

// Note that SLOT1_LEN, SLOT1_OFFSET and RIOTBOOT_HDR_LEN are given from Makefiles.
// See riot/sys/riotboot/Makefile.include and board-dropalt/Makefile.include.
constexpr uintptr_t SLOT1_IMAGE_OFFSET = (uintptr_t)SLOT1_OFFSET + RIOTBOOT_HDR_LEN;

const char* _reader(lua_State*, void* pdata, size_t* psize)
{
    bool& has_read = *(bool*)pdata;
    if ( unlikely(has_read) )
        return nullptr;

    has_read = true;
    *psize = riotboot_slot_get_hdr(SLOT1)->start_addr;
    constexpr size_t SLOT1_IMAGE_SIZE = SLOT1_LEN - RIOTBOOT_HDR_LEN;
    assert( *psize > 0 && *psize <= SLOT1_IMAGE_SIZE );
    // OR we could return all of slot 1 for reading. Lua would read only the compiled
    // code within it.
    // *psize = SLOT1_IMAGE_SIZE;

    return (const char*)SLOT1_IMAGE_OFFSET;
}

// ( -- )
static status_t load_keymap(lua_State* L)
{
    // Verify the validity of the slot header in slot 1.
    constexpr unsigned SLOT1 = 1;
    assert( riotboot_slot_validate(SLOT1) == 0 );

    // Verify that the image is a valid Lua bytecode.
    constexpr union {
        char str[5];
        uint32_t num;
    } signature = { LUA_SIGNATURE };
    assert( *(const uint32_t*)SLOT1_IMAGE_OFFSET == signature.num );

    bool has_read = false;
    // We pass a null chunkname argument to lua_load(). In this case, Lua will
    // assign the default value "=(load)" to the chunkname.
    status_t status = lua_load(L, _reader, &has_read, nullptr, "b");
    if ( status == LUA_OK ) {
        lua_pushstring(L, "keymap");  // "keymap" is the module name.
        status = lua_pcall(L, 1, 1, 0);
        if ( status != LUA_OK )
            l_message(lua_tostring(L, -1));
        lua_pop(L, 1);  // Remove the module table returned or the error message.
    }

    return status;
}



void init()
{
    L = lua_riot_newstate(lua_memory, sizeof(lua_memory), _panic);
    assert( L != nullptr );

    fputs(LUA_COPYRIGHT "\n", stdout);

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

    // Load the keymap module from flash memory.
    status_t status = load_keymap(L);

    // Runtime errors are not allowed during the module loading.
    assert( status == LUA_OK );
}

}
