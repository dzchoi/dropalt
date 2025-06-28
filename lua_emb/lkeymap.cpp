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
#include "map.hpp"              // for key::map_t::_delete(), ...
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

// keymap:press() -> void
// Note: Lua interacts solely with base map_t instances. Responsibility for handling
// derived types lies in C++.
static int _press(lua_State* L)
{
    key::map_t* obj = static_cast<key::map_t*>(luaL_checkudata(L, 1, "map_t"));
    obj->press();
    // ( -- keymap )
    return 0;
}

// keymap:release() -> void
static int _release(lua_State* L)
{
    key::map_t* obj = static_cast<key::map_t*>(luaL_checkudata(L, 1, "map_t"));
    obj->release();
    // ( -- keymap )
    return 0;
}

// keymap:is_pressed() -> bool
static int _is_pressed(lua_State* L)
{
    key::map_t* obj = static_cast<key::map_t*>(luaL_checkudata(L, 1, "map_t"));
    lua_pushboolean(L, obj->is_pressed());
    // ( -- keymap bool )
    return 1;
}

static int __gc(lua_State* L)
{
    // ( keymap -- )
    key::map_t* obj = static_cast<key::map_t*>(lua_touserdata(L, -1));
    obj->_delete();
    return 0;
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

    // Create a common metatable called "map_t" for all map_t and child instances.
    // Note: This must be created before invoking the keymap module below, as it depends
    // on the metatable.
    luaL_newmetatable(L, "map_t");
    lua_pushcfunction(L, __gc);
    lua_setfield(L, -2, "__gc");
    // ( -- chunk metatable )

    // Create the __index method table in the metatable.
    lua_newtable(L);
    lua_pushcfunction(L, _press);
    lua_setfield(L, -2, "press");
    lua_pushcfunction(L, _release);
    lua_setfield(L, -2, "release");
    lua_pushcfunction(L, _is_pressed);
    lua_setfield(L, -2, "is_pressed");
    // ( -- chunk metatable index-table )

    lua_setfield(L, -2, "__index");
    // ( -- chunk metatable )
    lua_pop(L, 1);
    // ( -- chunk )

    // Invoke the keymap module outside a protected environment; any runtime errors will
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

void handle_key_event(unsigned slot_index1, bool is_press)
{
    const char* const press_or_release = ::press_or_release(is_press);

    // Execute the event if in normal mode.
    if ( !::key::event_queue::deferrer() ) {
        LOG_DEBUG("Map: [%u] handle %s\n", slot_index1, press_or_release);

        lua_pushlightuserdata(L, (void*)&load_keymap);
        lua_gettable(L, LUA_REGISTRYINDEX);
        lua_pushinteger(L, slot_index1);
        lua_gettable(L, -2);  // Retrieve the keymap from module-table[slot_index1].
        lua_remove(L, -2);
        // ( -- userdata )

        // Invoke _press/_release() outside a protected environment; any runtime errors
        // will trigger `_panic(L)`.
        is_press ? _press(L) : _release(L);
        // ( -- userdata )

        return lua_settop(L, 0);
    }

    assert( false );
}

}
