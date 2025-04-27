// The "fw" module provides a Lua interface for the utility functions in the firmware.

#include "assert.h"
#include "board.h"              // for system_reset(), sam0_flashpage_aux_get(), ...
#include "log.h"                // for get/set_log_mask()
#include "thread.h"             // for thread_isr_stack_usage(), ...
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include "persistent.hpp"       // for persistent::get(), ...



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
    int x = luaL_checkinteger(L, 1);
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

    int mask = luaL_checkinteger(L, 1);
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
    const char* s = luaL_checklstring(L, 1, &len);
    // `struct Usb2422` restricts the iSerialNumber size to `uint16_t SERSTR[31]`.
    if ( s == nullptr || len > 31 )
        return luaL_argerror(L, 1,
            "product serial must be a string containing a maximum of 31 characters");

    sam0_flashpage_aux_write(0, s, len + 1);
    system_reset();
    return 0;
}

// Read an integer from NVM (e.g. fw.nvm.var).
// Note that if the stored value size is less than 4 bytes it will be loaded with zero-
// extension.
// ( nvm name -- value | nil )
static int _nvm_index(lua_State* L)
{
    const char* name = luaL_checkstring(L, -1);
    int value = 0;
    if ( persistent::get(name, value) )
        lua_pushinteger(L, value);
    else
        lua_pushnil(L);
    // "The function does not need to clear the stack before pushing its results. After
    // it returns, Lua automatically saves its results and clears its entire stack."
    // lua_insert(L, 1);
    // lua_settop(L, 1);
    return 1;
}

// Write an integer to NVM (e.g. fw.nvm.var = 27).
// Note that if the stored value size is less than 4 bytes, only the LSBs that fit within
// the size will be stored.
// ( nvm name value -- )
static int _nvm_newindex(lua_State* L)
{
    const char* name = luaL_checkstring(L, -2);
    if ( lua_isnil(L, -1) )
        // No need to trigger an error for deleting non-existent keys.
        persistent::remove(name);
    else {
        int value = luaL_checkinteger(L, -1);
        if ( !persistent::set(name, value) )
            luaL_error(L, "cannot add a new key");
    }
    return 0;
}

// An iterator invoked by `pairs(fw.nvm)` to traverse all entries in the table.
// ( nvm key -- next_key next_value | nil )
static int _nvm_iterator(lua_State* L)
{
    const char* name = nullptr;
    if ( !lua_isnil(L, -1) )
        name = luaL_checkstring(L, -1);

    name = persistent::next(name);
    if ( name == nullptr ) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, name);
    int value = 0;
    assert( persistent::get(name, value) );
    lua_pushinteger(L, value);
    return 2;
}

// ( nvm -- iterator nvm nil )
static int _nvm_pairs(lua_State* L)
{
    lua_pushcfunction(L, _nvm_iterator);
    lua_insert(L, 1);
    lua_pushnil(L);
    return 3;
}

// fw.nvm_clear() -> void
static int fw_nvm_clear(lua_State*)
{
    persistent::remove_all();
    return 0;
}

// fw.stack_usage() -> strings
static int fw_stack_usage(lua_State* L)
{
    lua_pushfstring(L, "ISR stack: %d / %d bytes",
        thread_isr_stack_usage(), ISR_STACKSIZE);
    for ( kernel_pid_t i = KERNEL_PID_FIRST ; i <= KERNEL_PID_LAST ; i++ ) {
        const thread_t* tcb = (thread_t*)sched_threads[i];
        if ( tcb != nullptr )
            // Note: thread_measure_stack_free() will not work if the thread was created
            // with the flag THREAD_CREATE_NO_STACKTEST.
            lua_pushfstring(L, "%s stack: %d / %d bytes",
                tcb->name,
                tcb->stack_size - thread_measure_stack_free(tcb),
                tcb->stack_size);
    }

    return lua_gettop(L);
}

int luaopen_fw(lua_State* L)
{
    static constexpr luaL_Reg fw_lib[] = {
        { "led0", fw_led0 },
        { "log_mask", fw_log_mask },
        { "nvm", nullptr },  // placeholder for the `nvm` table
        { "nvm_clear", fw_nvm_clear },
        { "product_serial", fw_product_serial },
        { "reboot_to_bootloader", fw_reboot_to_bootloader },
        { "stack_usage", fw_stack_usage },
        { "system_reset", fw_system_reset },
        { nullptr, nullptr }
    };

    luaL_newlib(L, fw_lib);
    (void)lua_newuserdata(L, 0);  // Create a new (full) userdata for the "nvm" object.

    lua_newtable(L);
    lua_pushcfunction(L, _nvm_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, _nvm_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, _nvm_pairs);
    lua_setfield(L, -2, "__pairs");
    lua_setmetatable(L, -2);

    lua_setfield(L, -2, "nvm");
    return 1;
}

}
