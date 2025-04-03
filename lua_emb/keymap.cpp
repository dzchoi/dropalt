#include "assert.h"
#include "compiler_hints.h"     // for unlikely()
#include "riotboot/slot.h"      // for riotboot_slot_get_hdr(), ...

extern "C" {
#include "lua.h"
}

#include "keymap.hpp"
#include "lua.hpp"              // for l_message()



namespace lua {

const char* keymap::_reader(lua_State*, void* arg, size_t* psize)
{
    uint32_t& slot1_image_size = *(uint32_t*)arg;
    if ( unlikely(slot1_image_size == 0) )
        return nullptr;

    *psize = slot1_image_size;
    slot1_image_size = 0;
    return (const char*)SLOT1_IMAGE_OFFSET;
}

status_t keymap::load_keymap(lua_State* L)
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
    // OR we could return all of slot 1 for reading. Lua would read only the compiled
    // code within it.
    // uint32_t slot1_image_size = SLOT1_IMAGE_SIZE;

    // We pass a null chunkname argument to lua_load(). In this case, Lua will assign
    // the default value "=(load)" to the chunkname.
    status_t status = lua_load(L, _reader, &slot1_image_size, nullptr, "b");
    if ( status == LUA_OK ) {
        lua_pushstring(L, "keymap");  // "keymap" is the module name.
        status = lua_pcall(L, 1, 1, 0);
        if ( status != LUA_OK )
            l_message(lua_tostring(L, -1));
        lua_pop(L, 1);  // Remove the module table returned or the error message.
    }

    return status;
}

}
