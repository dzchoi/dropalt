#include "assert.h"
#include "compiler_hints.h"     // for unlikely()
#include "log.h"
#include "riotboot/slot.h"      // for riotboot_slot_get_hdr(), ...

#include "lkeymap.hpp"
#include "lua.hpp"



namespace lua {

static const char* _reader(lua_State*, void* arg, size_t* psize)
{
    bool& done = *(bool*)arg;
    if ( unlikely(done) )
        return nullptr;

    // We return the data all at once.
    done = true;

    // Note that SLOT*_LEN, SLOT*_OFFSET and RIOTBOOT_HDR_LEN are given from Makefiles.
    // See riot/sys/riotboot/Makefile.include and board-dropalt/Makefile.include.
    constexpr unsigned SLOT0 = 0;

    // Verify that the slot 0 contains a header and it contains a valid Lua bytecode.
    constexpr uintptr_t addr = SLOT0_OFFSET + RIOTBOOT_HDR_LEN;
    assert( riotboot_slot_validate(SLOT0) == 0
        && global_lua_state::validate_bytecode(addr) );

    // We interpret header->start_addr as the bytecode size.
    *psize = riotboot_slot_get_hdr(SLOT0)->start_addr;
    // Or we could return the entire contents in slot 0 for reading. Lua will extract
    // only the compiled code portion.
    // *psize = SLOT0_LEN - RIOTBOOT_HDR_LEN;
    assert( *psize > 0 && *psize <= SLOT0_LEN - RIOTBOOT_HDR_LEN );

    return (const char*)addr;
}

void load_keymap()
{
    global_lua_state L;

    bool done = false;
    // We pass a null chunkname argument to lua_load(). In this case, Lua will assign
    // the default value "=(load)" to the chunkname.
    status_t status = lua_load(L, _reader, &done, nullptr, "b");
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

    // Store the Lua lamp driver in the registry under the key `&handle_lamp_state`.
    lua_pushlightuserdata(L, (void*)&handle_lamp_state);
    type = lua_rawgeti(L, -2, 2);  // Retrieve the second entry in the table.
    assert( type == LUA_TFUNCTION );
    // ( -- module-table &handle_lamp_state handle_lamp_state )
    lua_settable(L, LUA_REGISTRYINDEX);
    // ( -- module-table )

    // Clean up the local objects in the module that are no longer referenced.
    lua_gc(L, LUA_GCCOLLECT, 0);
    LOG_DEBUG("Lua: current memory usage = %d KB", lua_gc(L, LUA_GCCOUNT, 0));
    lua_settop(L, 0);
}

void handle_key_event(unsigned slot_index, bool is_press)
{
    global_lua_state L;

    lua_pushlightuserdata(L, (void*)&handle_key_event);
    lua_gettable(L, LUA_REGISTRYINDEX);
    // ( -- handle_key_event )
    lua_pushinteger(L, slot_index);
    lua_pushboolean(L, is_press);
    // ( -- handle_key_event slot_index is_press )
    lua_call(L, 2, 0);  // Invoke handle_key_event() outside a protected environment.
    // ( -- )
}

void handle_lamp_state(uint8_t lamp_state)
{
    global_lua_state L;

    lua_pushlightuserdata(L, (void*)&handle_lamp_state);
    lua_gettable(L, LUA_REGISTRYINDEX);
    // ( -- handle_lamp_state )
    lua_pushinteger(L, lamp_state);
    // ( -- handle_lamp_state lamp_state )
    lua_call(L, 1, 0);  // Invoke handle_lamp_state() outside a protected environment.
    // ( -- )
}

}
