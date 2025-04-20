// The "fw" module provides a Lua interface for the utility functions in the firmware.

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include "board.h"              // for fw_system_reset(), ...
#include "log.h"                // for get/set_log_mask()

#include "lua.hpp"



namespace lua {

// fw.system_reset()
static int fw_system_reset(lua_State*)
{
    system_reset();
    return 0;
}

// fw.reboot_to_bootloader()
static int fw_reboot_to_bootloader(lua_State*)
{
    reboot_to_bootloader();
    return 0;
}

// fw.led0() -> int
// fw.led0(int x) -> void
static int fw_led0(lua_State* L)
{
    if ( lua_gettop(L) == 0 ) {
        lua_pushinteger(L, gpio_read(LED0_PIN));
        return 1;
    }

    // Check for a boolean explicitly since true does not convert to 1.
    int x = lua_tointeger(L, 1);
    if ( x == 0 )
        LED0_OFF;
    else if ( x > 0 )
        LED0_ON;
    else
        LED0_TOGGLE;
    return 0;
}

// fw.log_mask() -> int
// fw.log_mask(int mask) -> void
static int fw_log_mask(lua_State* L)
{
    if ( lua_gettop(L) == 0 ) {
        lua_pushinteger(L, get_log_mask());
        return 1;
    }

    int mask = lua_tointeger(L, 1);
    set_log_mask(static_cast<uint8_t>(mask));
    return 0;
}

// fw.product_serial() -> string
// fw.product_serial(string s) -> (system reset)
// Note that it cannot overwrite the existing product serial as it does not erase the
// USER page.
static int fw_product_serial(lua_State* L)
{
    char* p = (char*)sam0_flashpage_aux_get(0);
    if ( lua_gettop(L) == 0 ) {
        lua_pushstring(L, *p == '\xff' ? "(null)" : p);
        return 1;
    }

    if ( *p != '\xff' )
        return luaL_error(L, "product serial already exists");

    size_t len;
    const char* s = lua_tolstring(L, 1, &len);
    // `struct Usb2422` restricts the iSerialNumber size to `uint16_t SERSTR[31]`.
    if ( s == nullptr || len > 31 )
        return luaL_argerror(L, 1,
            "product serial must be a string containing a maximum of 31 characters");

    sam0_flashpage_aux_write(0, s, len + 1);
    system_reset();
    return 0;
}

int luaopen_fw(lua_State* L)
{
    static constexpr luaL_Reg fw_lib[] = {
        { "led0", fw_led0 },
        { "log_mask", fw_log_mask },
        { "product_serial", fw_product_serial },
        { "reboot_to_bootloader", fw_reboot_to_bootloader },
        { "system_reset", fw_system_reset },
        { NULL, NULL }
    };

    luaL_newlib(L, fw_lib);
    return 1;
}

}
