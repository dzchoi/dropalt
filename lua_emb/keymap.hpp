#pragma once

#include <cstddef>              // for size_t
#include <cstdint>              // for uintptr_t

#include "lua.hpp"              // for lua_State, status_t



namespace lua {

class keymap {
public:
    // Load the "keymap" module.
    // Note: It is required that the module itself places its module table into
    // package.loaded[].
    // ( -- )
    static status_t load_keymap(lua_State* L);

    // static status_t handle_key_event();

private:
    constexpr keymap() =delete;  // Ensure a static class

    static constexpr unsigned SLOT1 = 1;

    // Note that SLOT1_LEN, SLOT1_OFFSET and RIOTBOOT_HDR_LEN are given from Makefiles.
    // See riot/sys/riotboot/Makefile.include and board-dropalt/Makefile.include.
    static constexpr uintptr_t SLOT1_IMAGE_OFFSET =
        (uintptr_t)SLOT1_OFFSET + RIOTBOOT_HDR_LEN;
    static constexpr size_t SLOT1_IMAGE_SIZE = SLOT1_LEN - RIOTBOOT_HDR_LEN;

    static const char* _reader(lua_State* L, void* pdata, size_t* psize);
};

}
