// The "fw" module exposes firmware utility functions to Lua.

#include "board.h"              // for system_reset(), sam0_flashpage_aux_get(), ...
#include "log.h"                // for get/set_log_mask()
#include "thread.h"             // for thread_isr_stack_usage(), ...
extern "C" {
// #include "lua.h"
#include "lauxlib.h"
}

#include "hid_keycodes.hpp"     // for keycode_to_name[]
#include "key_queue.hpp"        // for key_queue::start_defer(), ...
#include "persistent.hpp"       // for persistent::_get/_set(), ...
#include "timer.hpp"            // for _timer_t::create(), ...
#include "usb_thread.hpp"       // for usb_thread::send_press/release()



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

// fw.log(string) -> void
static int fw_log(lua_State* L)
{
    const char* message = luaL_checkstring(L, 1);
    LOG_DEBUG("%s\n", message);
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
    luaL_argcheck(L, (len <= 31), 1,
        "product serial must be a string containing a maximum of 31 characters");

    sam0_flashpage_aux_write(0, s, len + 1);
    system_reset();
    return 0;
}

// Read from NVM (e.g. print(fw.nvm.var)).
// ( nvm name -- value | nil )
int _nvm_index(lua_State* L)
{
    const char* name = luaL_checkstring(L, -1);
    persistent::lock_guard nvm_lock;
    auto it = persistent::_find(name);

    if ( it == persistent::end() )
        lua_pushnil(L);

    else
    switch ( it->value_type ) {
        case persistent::TSTRING:
            lua_pushstring(L, (char*)it.pvalue());
            break;

        case persistent::TFLOAT: {
            float value;
            persistent::_get(name, &value, sizeof(float));
            lua_pushnumber(L, value);
            break;
        }

        // For a value_type other than TSTRING and TFLOAT, read it as an int, zero-extend
        // if smaller than 4 bytes, or take only the first 4 bytes if larger.
        default: {
            int value = 0;
            persistent::_get(name, &value, sizeof(int));
            lua_pushinteger(L, value);
            break;
        }
    }

    // "The function does not need to clear the stack before pushing its results. After
    // it returns, Lua automatically saves its results and clears its entire stack."
    // lua_insert(L, 1);
    // lua_settop(L, 1);
    return 1;
}

// Write to NVM (e.g. fw.nvm.var = 27).
// ( nvm name value -- )
int _nvm_newindex(lua_State* L)
{
    const char* name = luaL_checkstring(L, -2);
    persistent::lock_guard::lock();
    auto it = persistent::_find(name);

    if ( lua_isnil(L, -1) ) {
        // Do not trigger an error for deleting non-existent keys.
        persistent::_remove(it);
        return 0;
    }

    bool success = false;
    if ( lua_isinteger(L, -1) ) {
        int value = lua_tointeger(L, -1);
        if ( it == persistent::end() )
            success = persistent::_create(name, &value, sizeof(value), sizeof(int));

        else
        switch ( it->value_type ) {
            case persistent::TSTRING:
            case persistent::TFLOAT:
                success = persistent::_remove(it)
                    && persistent::_create(name, &value, sizeof(value), sizeof(int));
                break;

            // If the stored value size is less than 4 bytes, only the LSBs that fit
            // within the size will be stored.
            default:
                success = persistent::_set(name, &value, sizeof(value));
                break;
        }
    }

    else if ( lua_isnumber(L, -1) ) {
        float value = lua_tonumber(L, -1);
        if ( it == persistent::end() )
            success = persistent::_create(
                name, &value, sizeof(value), persistent::TFLOAT);

        else
        switch ( it->value_type ) {
            case persistent::TFLOAT:
                success = persistent::_set(name, &value, sizeof(value));
                break;

            default:
                success = persistent::_remove(it)
                    && persistent::_create(
                        name, &value, sizeof(value), persistent::TFLOAT);
                break;
        }
    }

    else if ( lua_isstring(L, -1) ) {
        const char* value = lua_tostring(L, -1);
        persistent::_remove(it);
        success = persistent::_create(
            name, value, __builtin_strlen(value) + 1, persistent::TSTRING);
    }

    else {
        // Unlock the mutex manually before luaL_argerror() terminates the function.
        persistent::lock_guard::unlock();
        luaL_argerror(L, 3, "type not allowed in NVM");
    }

    persistent::lock_guard::unlock();
    if ( !success )
        luaL_error(L, "cannot add a new key to NVM");
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
    lua_pop(L, 1);
    if ( name == nullptr ) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, name);
    return 1 + _nvm_index(L);  // Return (name, value) pair.
}

// ( nvm -- iterator nvm nil )
static int _nvm_pairs(lua_State* L)
{
    lua_pushcfunction(L, _nvm_iterator);
    lua_insert(L, 1);
    lua_pushnil(L);
    return 3;
}

// Number of entries in the NVM.
// ( nvm -- count )
static int _nvm_len(lua_State* L)
{
    lua_pushinteger(L, persistent::size());
    return 1;
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

// fw.keycode(string keyname) -> int | nil
// Note: This uses an unoptimized linear search to find the keyname.
static int fw_keycode(lua_State* L)
{
    const char* keyname = luaL_checkstring(L, 1);
    constexpr int NUM_KEYCODES = sizeof(keycode_to_name) / sizeof(keycode_to_name[0]);

    int result = KC_NO;  // KC_NO (= 0) is not a valid keycode.
    for ( int keycode = KC_A ; keycode < NUM_KEYCODES ; keycode++ )
        if ( __builtin_strcmp(keyname, keycode_to_name[keycode]) == 0 ) {
            result = keycode;
            break;
        }

    lua_pushinteger(L, result);
    return result != KC_NO;
}

// fw.send_key(int keycode, bool is_press) -> void
static int fw_send_key(lua_State* L)
{
    uint8_t keycode = luaL_checkinteger(L, 1);
    if ( lua_toboolean(L, 2) )
        usb_thread::send_press(keycode);
    else
        usb_thread::send_release(keycode);
    return 0;
}

int luaopen_fw(lua_State* L)
{
    static constexpr luaL_Reg fw_lib[] = {
        { "defer_start", key_queue::defer_start },
        { "defer_stop", key_queue::defer_stop },
        { "defer_owner", key_queue::defer_owner },
        { "defer_is_pending", key_queue::defer_is_pending },
        { "defer_remove_last", key_queue::defer_remove_last },
        { "keycode", fw_keycode },
        { "led0", fw_led0 },
        { "log", fw_log },
        { "log_mask", fw_log_mask },
        { "nvm", nullptr },  // placeholder for the `nvm` table
        { "nvm_clear", fw_nvm_clear },
        { "product_serial", fw_product_serial },
        { "reboot_to_bootloader", fw_reboot_to_bootloader },
        { "send_key", fw_send_key },
        { "stack_usage", fw_stack_usage },
        { "system_reset", fw_system_reset },
        { "timer_create", _timer_t::create },
        { "timer_is_running", _timer_t::is_running },
        { "timer_start", _timer_t::start },
        { "timer_stop", _timer_t::stop },
        { nullptr, nullptr }
    };

    luaL_newlib(L, fw_lib);
    (void)lua_newuserdata(L, 0);  // Create a new (full) userdata for the "nvm" object.

    lua_newtable(L);
    lua_pushcfunction(L, _nvm_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, _nvm_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, _nvm_len);
    lua_setfield(L, -2, "__len");
    lua_pushcfunction(L, _nvm_pairs);
    lua_setfield(L, -2, "__pairs");
    lua_setmetatable(L, -2);

    lua_setfield(L, -2, "nvm");
    return 1;
}

}
