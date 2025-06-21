#include "assert.h"
#include "compiler_hints.h"     // for unlikely()
#include "log.h"
#include "riotboot/slot.h"      // for riotboot_slot_get_hdr(), ...

extern "C" {
#include "lua.h"
#include "lauxlib.h"            // declares luaL_*().
}

#include "key_event_queue.hpp"  // for key::event_queue::deferrer(), ...
#include "lkeymap.hpp"
#include "main_thread.hpp"      // for press_or_release()
#include "map.hpp"              // for key::map_t::__gc(), ...
#include "literal.hpp"          // for key::literal_t::__gc(), ...
#include "lua.hpp"              // for lua::L, l_message()



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

// keymap::press(int slot_index) -> void
static int _press(lua_State* L)
{
    // Negative stack indices are used to allow this function to be called from C API.
    key::map_t* obj = static_cast<key::map_t*>(lua_touserdata(L, -2));
    luaL_argcheck(L, (obj != nullptr), 1, "not a map_t instance");
    obj->press(luaL_checkinteger(L, -1));
    return 0;
}

// keymap::release(int slot_index) -> void
static int _release(lua_State* L)
{
    key::map_t* obj = static_cast<key::map_t*>(lua_touserdata(L, -2));
    luaL_argcheck(L, (obj != nullptr), 1, "not a map_t instance");
    obj->release(luaL_checkinteger(L, -1));
    return 0;
}

// keymap:is_pressed() -> bool
static int _is_pressed(lua_State* L)
{
    key::map_t* obj = static_cast<key::map_t*>(lua_touserdata(L, -1));
    luaL_argcheck(L, (obj != nullptr), 1, "not a map_t instance");
    lua_pushboolean(L, obj->is_pressed());
    return 1;
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

    // Create a common __index method table for all map_t and child instances.
    lua_newtable(L);
    lua_pushcfunction(L, _press);
    lua_setfield(L, -2, "press");
    lua_pushcfunction(L, _release);
    lua_setfield(L, -2, "release");
    lua_pushcfunction(L, _is_pressed);
    lua_setfield(L, -2, "is_pressed");
    // ( -- chunk index-table )

    // Create a metatable for map_t userdata.
    luaL_newmetatable(L, "map_t");
    lua_pushcfunction(L, key::map_t::__gc);
    lua_setfield(L, -2, "__gc");
    // ( -- chunk index-table metatable )
    lua_pushvalue(L, -2);
    // ( -- chunk index-table metatable index-table )
    lua_setfield(L, -2, "__index");
    // ( -- chunk index-table metatable )
    lua_pop(L, 1);
    // ( -- chunk index-table )

    // Create a metatable for literal_t userdata.
    luaL_newmetatable(L, "literal_t");
    lua_pushcfunction(L, key::literal_t::__gc);
    lua_setfield(L, -2, "__gc");
    // ( -- chunk index-table metatable )
    lua_insert(L, -2);
    // ( -- chunk metatable index-table )
    lua_setfield(L, -2, "__index");
    // ( -- chunk metatable )
    lua_pop(L, 1);
    // ( -- chunk )

    // Invoke the keymap module outside a protected environment; runtime errors will
    // trigger `_panic(L)`.
    lua_pushstring(L, "keymap");  // "keymap" is the module name.
    lua_call(L, 1, 1);
    // The module should return a module table.
    assert( lua_istable(L, -1) );
    // ( -- module-table )

    // Store the module table in the registry using the unique key `&load_keymap`..
    lua_pushlightuserdata(L, (void*)&load_keymap);
    lua_insert(L, -2);  // Swap the top two values.
    // ( -- &load_keymap module-table )
    lua_settable(L, LUA_REGISTRYINDEX);
    // ( -- )
}

void handle_key_event(unsigned slot_index, bool is_press)
{
    const char* const press_or_release = ::press_or_release(is_press);

    // Execute the event if in normal mode.
    if ( !::key::event_queue::deferrer() ) {
        LOG_DEBUG("Keymap: [%u] handle %s\n", slot_index, press_or_release);
        // rgb_thread::obj().signal_key_event(slot_index, is_press);

        lua_pushlightuserdata(L, (void*)&load_keymap);
        lua_gettable(L, LUA_REGISTRYINDEX);
        lua_pushinteger(L, slot_index);
        lua_gettable(L, -2);  // Get the userdata (keymap) from keymap[slot_index].
        // ( -- module-table keymap )

        lua_pushinteger(L, slot_index);
        // ( -- module-table keymap slot_index )
        // _press/_release() are invoked outside a protected environment; any runtime
        // errors will trigger `_panic(L)`.
        is_press ? _press(L) : _release(L);
        // ( -- module-table keymap slot_index )

        lua_settop(L, 0);
        return;
    }

    assert( false );
}

}
