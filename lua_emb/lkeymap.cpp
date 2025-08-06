#include "assert.h"
#include "compiler_hints.h"     // for unlikely()
#include "log.h"
#include "riotboot/slot.h"      // for riotboot_slot_get_hdr(), ...

#include "lkeymap.hpp"
#include "lua.hpp"              // for lua::L, status_t



namespace lua {

static constexpr unsigned SLOT1 = 1;

// Note that SLOT1_LEN, SLOT1_OFFSET and RIOTBOOT_HDR_LEN are given from Makefiles.
// See riot/sys/riotboot/Makefile.include and board-dropalt/Makefile.include.
static constexpr uintptr_t SLOT1_IMAGE_OFFSET =
    (uintptr_t)SLOT1_OFFSET + RIOTBOOT_HDR_LEN;
static constexpr size_t SLOT1_IMAGE_SIZE = SLOT1_LEN - RIOTBOOT_HDR_LEN;


static const char* _reader(lua_State*, void* arg, size_t* psize)
{
    uint32_t& slot1_image_size = *(uint32_t*)arg;
    if ( unlikely(slot1_image_size == 0) )
        return nullptr;

    *psize = slot1_image_size;
    slot1_image_size = 0;
    return (const char*)SLOT1_IMAGE_OFFSET;
}

void load_keymap()
{
    // Verify the validity of the slot header in slot 1.
    assert( riotboot_slot_validate(SLOT1) == 0 );

    // Verify that the image is a valid Lua bytecode.
    constexpr union {
        char str[5];
        uint32_t num;
    } signature = { LUA_SIGNATURE };
    assert( *(uint32_t*)SLOT1_IMAGE_OFFSET == signature.num );

    // Verify the bytecode size.
    uint32_t slot1_image_size = riotboot_slot_get_hdr(SLOT1)->start_addr;
    assert( slot1_image_size > 0 && slot1_image_size <= SLOT1_IMAGE_SIZE );
    // Or we could return the entire contents in slot 1 for reading. Lua will read only
    // the compiled code within it.
    // uint32_t slot1_image_size = SLOT1_IMAGE_SIZE;

    // We pass a null chunkname argument to lua_load(). In this case, Lua will assign
    // the default value "=(load)" to the chunkname.
    status_t status = lua_load(L, _reader, &slot1_image_size, nullptr, "b");
    if ( status != LUA_OK )
        lua_error(L);  // Trigger _panic(L) with the error message.
    // ( -- chunk )

    // Invoke the keymap module outside a protected environment; any runtime error will
    // trigger `_panic(L)`.
    lua_pushstring(L, "keymap");  // "keymap" is the module name.
    lua_call(L, 1, 1);
    // The module should return a module table.
    assert( lua_istable(L, -1) );
    // ( -- module-table )

    // Store the Lua keymap driver in the registry under the key `&hadle_key_event`.
    lua_pushlightuserdata(L, (void*)&handle_key_event);
    int type = lua_rawgeti(L, -2, 1);  // Retrieve the first entry in the table.
    assert( type == LUA_TFUNCTION );
    // ( -- module-table &handle_key_event handle_key_event )
    lua_settable(L, LUA_REGISTRYINDEX);
    // ( -- module-table )

    // Clean up the local objects in the module that are no longer referenced.
    lua_gc(L, LUA_GCCOLLECT, 0);
    LOG_DEBUG("Lua: current memory usage = %d KB\n", lua_gc(L, LUA_GCCOUNT, 0));
    lua_settop(L, 0);
}

void handle_key_event(unsigned slot_index, bool is_press)
{
    lua_pushlightuserdata(L, (void*)&handle_key_event);
    lua_gettable(L, LUA_REGISTRYINDEX);
    // ( -- handle_key_event )
    lua_pushinteger(L, slot_index);
    lua_pushboolean(L, is_press);
    // ( -- handle_key_event slot_index is_press )
    lua_call(L, 2, 0);  // Invoke handle_key_event() outside a protected environment.
    // ( -- )
}

}
