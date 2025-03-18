// The "fw" module provides a Lua interface for the utility functions in the firmware.

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include "board.h"              // for fw_system_reset(), ...

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

// fw.led0(int x) -> void
static int fw_led0(lua_State* L)
{
    int x = lua_tointeger(L, 1);
    if ( x == 0 )
        LED0_OFF;
    else if ( x == 1 )
        LED0_ON;
    else
        LED0_TOGGLE;

    return 0;
}

int luaopen_fw(lua_State* L)
{
    static constexpr luaL_Reg fw_lib[] = {
        { "led0", fw_led0 },
        { "reboot_to_bootloader", fw_reboot_to_bootloader },
        { "system_reset", fw_system_reset },
        { NULL, NULL }
    };

    luaL_newlib(L, fw_lib);
    return 1;
}

}
