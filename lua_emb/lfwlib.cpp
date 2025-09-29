// The "fw" module exposes firmware utility functions to Lua.

#include "board.h"              // for system_reset(), sam0_flashpage_aux_get(), ...
#include "compiler_hints.h"     // for UNREACHABLE()
#include "is31fl3733.h"         // for is31_set_color(), IS31_LEDS, ...
#include "log.h"                // for get/set_log_mask()
#include "thread.h"             // for thread_isr_stack_usage(), ...
#include "usb2422.h"            // for usbhub_host_port()

#include "hid_keycodes.hpp"     // for keycode_to_name[]
#include "hsv.hpp"              // for fast_hsv2rgb_32bit(), CIE1931_CURVE[]
#include "key_queue.hpp"        // for key_queue::start_defer(), ...
#include "lexecute.hpp"         // for execute_later()
#include "lua.hpp"
#include "persistent.hpp"       // for persistent::_get/_set(), ...
#include "timer.hpp"            // for _timer_t::create(), ...
#include "usb_thread.hpp"       // for usb_thread::send_press/release()
#include "usbhub_thread.hpp"    // for usbhub_thread::request_usbport_switchover()



namespace lua {

static int fw_system_reset(lua_State*)
{
    system_reset();
    return 0;
}

static int fw_enter_bootloader(lua_State*)
{
    uint8_t current_port = usbhub_host_port();
    // Let the bootloader acquire the current host port.
    if ( current_port != USB_PORT_UNKNOWN ) {
        persistent::set("last_host_port", current_port);
        persistent::commit_now();
    }

    enter_bootloader();
    return 0;
}

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

static int fw_led_set_rgb(lua_State* L)
{
    int led_index = luaL_checkinteger(L, 1);
    luaL_argcheck(L, (led_index > 0 && led_index <= (int)ALL_LED_COUNT), 1,
        "invalid led_index");

    uint8_t r = luaL_checkinteger(L, 2);
    uint8_t g = luaL_checkinteger(L, 3);
    uint8_t b = luaL_checkinteger(L, 4);

    is31_set_color(IS31_LEDS[led_index - 1], r, g, b);
    return 0;
}

static int fw_led_set_hsv(lua_State* L)
{
    int led_index = luaL_checkinteger(L, 1);
    luaL_argcheck(L, (led_index > 0 && led_index <= (int)ALL_LED_COUNT), 1,
        "invalid led_index");

    // References for implementating hsv-to-rgb:
    //  - https://www.vagrearg.org/content/hsvrgb
    //  - https://cs.stackexchange.com/questions/64549/convert-hsv-to-rgb-colors
    //  - https://stackoverflow.com/a/24153899/10451825
    //  - https://programmingfbf7290.tistory.com/entry/%EC%83%89-%EA%B3%B5%EA%B0%843-HSV-HSL

    uint16_t h = luaL_checkinteger(L, 2);
    uint8_t s = luaL_checkinteger(L, 3);
    uint8_t v = luaL_checkinteger(L, 4);
    uint8_t r, g, b;
    // Use CIE 1931 curve to adjust intensity (v) for smoother perceptual transitions.
    fast_hsv2rgb_32bit(h, s, CIE1931_CURVE[v], &r, &g, &b);

    is31_set_color(IS31_LEDS[led_index - 1], r, g, b);
    return 0;
}

static int fw_led_refresh(lua_State*)
{
    is31_refresh_colors();
    return 0;
}

static intptr_t loggable_value(lua_State* L, int i)
{
    switch ( lua_type(L, i) ) {
        case LUA_TNIL:
            return -1;

        case LUA_TBOOLEAN:
            return lua_toboolean(L, i);

        case LUA_TNUMBER:
            if ( lua_isinteger(L, i) )
                return lua_tointeger(L, i);
            else
                // This converts a float to an integer numerically.
                // Note that even if we preserved the float's 4-byte bit pattern by
                // casting to intptr_t, '%f' formatting would not work as expected and
                // would print 0.0, garbage, or misinterpret memory. This is because in
                // variadic functions like log_backup(), C promotes float arguments to
                // 8-byte doubles (per Default Argument Promotion Rules), even on 32-bit
                // systems like Cortex-M4F. This behavior remains true even when using
                // printf_float() from newlib-nano.
                return static_cast<int>(lua_tonumber(L, i));

        case LUA_TSTRING:
            return reinterpret_cast<intptr_t>(lua_tostring(L, i));

        default:
            return reinterpret_cast<intptr_t>(lua_topointer(L, i));
    }
}

static int fw_log(lua_State* L)
{
#if !( defined(__ARM_ARCH) && (__ARM_ARCH >= 7) && defined(__thumb__) )
    // Note: This function assumes a 32-bit ARM architecture with support for the
    // Thumb-2 instruction set (typically ARMv7-A/M). It relies on AAPCS (ARM
    // Architecture Procedure Call Standard), which passes the first four function
    // arguments in r0–r3 and any additional arguments via the stack.
    #error "fw_log() requires Thumb-2 and ARMv7 or newer architecture"
#endif

    const char* format = luaL_checkstring(L, 1);
    int argc = lua_gettop(L);

    // Push extra arguments (from index 4 onward) onto the C stack. These arguments will
    // be accessed by log_backup() beyond r0–r3.
    const int extras = argc - 3;
    if ( extras > 0 ) {
        __asm__ volatile (
            "sub sp, sp, %[sz]\n"
            :
            : [sz] "r" (extras * sizeof(intptr_t))
        );

        while ( argc > 3 ) {
            __asm__ volatile (
                "str %[val], [sp, %[offset]]\n"
                :
                : [val] "r" (loggable_value(L, argc)),
                  [offset] "r" ((argc - 4) * sizeof(intptr_t))
            );
            argc--;
        }
    }

    // Call log_backup(LOG_DEBUG, ...) via inline assembly. The first four Arguments are
    // passed in r0–r3 according to AAPCS.
    __asm__ volatile (
        "mov r0, %[level]\n"
        "mov r1, %[a1]\n"
        "mov r2, %[a2]\n"
        "mov r3, %[a3]\n"
        "bl log_backup\n"
        :
        : [level] "I" (LOG_DEBUG),  // immediate constant
          [a1] "r" (format),
          [a2] "r" (loggable_value(L, 2)),
          [a3] "r" (loggable_value(L, 3))
        : "r0", "r1", "r2", "r3", "lr"
    );

    // Restore the C stack.
    if ( extras > 0 ) {
        __asm__ volatile (
            "add sp, sp, %[sz]\n"
            :
            : [sz] "r" (extras * sizeof(intptr_t))
        );
    }

    lua_settop(L, 0);
    return 0;
}

static int fw_log_mask(lua_State* L)
{
    if ( lua_gettop(L) == 0 ) {
        lua_pushinteger(L, get_log_mask());
        return 1;
    }

    int mask = luaL_checkinteger(L, 1);
    set_log_mask(static_cast<uint_fast8_t>(mask));
    return 0;
}

// Excerpt from table.pack() in ltablib.c
// static int fw_pack(lua_State* L)
// {
//     int i;
//     int n = lua_gettop(L);      // number of elements to pack
//     lua_createtable(L, n, 1);   // create result table
//     lua_insert(L, 1);           // put it at index 1
//     for (i = n; i >= 1; i--)    // assign elements
//         lua_seti(L, 1, i);
//     lua_pushinteger(L, n);
//     lua_setfield(L, 1, "n");    // t.n = number of elements
//     return 1;                   // return table
// }
// Alternatively,
// function fw.pack(...)
//     local t = {...}
//     t.n = select("#", ...)
//     return t
// end

// Excerpt from table.unpack() in ltablib.c
// static int fw_unpack(lua_State* L)
// {
//     lua_Unsigned n;
//     lua_Integer i = luaL_optinteger(L, 2, 1);
//     lua_Integer e = luaL_opt(L, luaL_checkinteger, 3, luaL_len(L, 1));
//     if (i > e) return 0;        // empty range
//     n = (lua_Unsigned)e - i;    // number of elements minus 1 (avoid overflows)
//     if (n >= (unsigned int)INT_MAX  || !lua_checkstack(L, (int)(++n)))
//         return luaL_error(L, "too many results to unpack");
//     for (; i < e; i++) {        // push arg[i..e - 1] (to avoid overflows)
//         lua_geti(L, 1, i);
//     }
//     lua_geti(L, 1, e);          // push last element
//     return (int)n;
// }
// Alternatively,
// function fw.unpack(t, i, j)
//     i = i or 1
//     j = j or #t
//     if i > j then
//         return
//     else
//         return t[i], fw.unpack(t, i + 1, j)
//     end
// end

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

int _nvm_index(lua_State* L)
{
    const char* name = luaL_checkstring(L, -1);
    persistent::lock_guard nvm_lock;
    auto it = persistent::_find(name);

    if ( it == persistent::end() )
        return 0;

    switch ( it->value_type ) {
        case persistent::TSTRING:
            lua_pushstring(L, (char*)it.to_value());
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
    return 1;
}

int _nvm_newindex(lua_State* L)
{
    const char* name = luaL_checkstring(L, -2);
    persistent::lock_guard::lock();
    auto it = persistent::_find(name);

    bool success = false;
    switch ( lua_type(L, -1) ) {
        case LUA_TNIL:
            persistent::_remove(it);
            // Do not trigger an error for deleting non-existent keys.
            success = true;
            break;

        case LUA_TNUMBER:
            if ( lua_isinteger(L, -1) ) {
                int value = lua_tointeger(L, -1);
                if ( it == persistent::end() )
                    success = persistent::_create(
                        name, &value, sizeof(value), sizeof(int));

                else if ( it->value_type == persistent::TSTRING
                       || it->value_type == persistent::TFLOAT )
                    success = persistent::_remove(it)
                        && persistent::_create(name, &value, sizeof(value), sizeof(int));

                else
                    // If the stored value size is less than 4 bytes, only the LSBs that
                    // fit within the size will be stored.
                    success = persistent::_set(name, &value, sizeof(value));
            }

            else {
                float value = lua_tonumber(L, -1);
                if ( it == persistent::end() )
                    success = persistent::_create(
                        name, &value, sizeof(value), persistent::TFLOAT);

                else if ( it->value_type == persistent::TFLOAT )
                    success = persistent::_set(name, &value, sizeof(value));

                else
                    success = persistent::_remove(it)
                        && persistent::_create(
                            name, &value, sizeof(value), persistent::TFLOAT);
            }
            break;

        case LUA_TSTRING: {
            const char* value = lua_tostring(L, -1);
            persistent::_remove(it);
            success = persistent::_create(
                name, value, __builtin_strlen(value) + 1, persistent::TSTRING);
            break;
        }

        default:
            // Unlock the mutex manually before luaL_argerror() terminates the function.
            persistent::lock_guard::unlock();
            luaL_argerror(L, 3, "type not allowed in NVM");
            UNREACHABLE();
    }

    persistent::lock_guard::unlock();
    if ( !success ) {
        luaL_error(L, "cannot add a new key to NVM");
        UNREACHABLE();
    }
    return 0;
}

// _nvm_iterator(nvm, name: string): (string, value) | void
// An iterator (a.k.a. next function) returned from `fw.nvm.__pairs(fw.nvm)` to traverse
// all entries in the `nvm` table.
static int _nvm_iterator(lua_State* L)
{
    const char* name = nullptr;
    if ( !lua_isnil(L, -1) )
        name = luaL_checkstring(L, -1);

    name = persistent::next(name);
    lua_pop(L, 1);
    if ( name == nullptr )
        return 0;

    lua_pushstring(L, name);
    return 1 + _nvm_index(L);  // Return (name, value) pair.
}

static int _nvm_pairs(lua_State* L)
{
    lua_pushcfunction(L, _nvm_iterator);
    lua_insert(L, 1);
    lua_pushnil(L);
    return 3;
}

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

static int fw_keycode(lua_State* L)
{
    const char* keyname = luaL_checkstring(L, 1);
    constexpr int NUM_KEYCODES = sizeof(keycode_to_name) / sizeof(keycode_to_name[0]);

    // Note that we use a simple linear search to look up the keyname.
    int result = KC_NO;  // KC_NO (= 0) is not a valid keycode.
    for ( int keycode = KC_A ; keycode < NUM_KEYCODES ; keycode++ )
        if ( __builtin_strcmp(keyname, keycode_to_name[keycode]) == 0 ) {
            result = keycode;
            break;
        }

    if ( result == KC_NO ) {
        luaL_error(L, "invalid keyname \"%s\"", keyname);
        UNREACHABLE();
    }
    lua_pushinteger(L, result);
    return 1;
}

static int fw_send_key(lua_State* L)
{
    uint8_t keycode = luaL_checkinteger(L, 1);
    if ( lua_toboolean(L, 2) )
        usb_thread::send_press(keycode);
    else
        usb_thread::send_release(keycode);
    return 0;
}

static int fw_switchover(lua_State*)
{
    usbhub_thread::request_usbport_switchover();
    return 0;
}



int luaopen_fw(lua_State* L)
{
    static constexpr luaL_Reg fw_lib[] = {
// fw.defer_start(): void
// Starts defer mode.
// The `fw.defer_*()` functions support the Defer class implementation. See comments in
// core.lua for more details on defer mode operations.
        { "defer_start", key_queue::defer_start },

// fw.defer_stop(): void
// Stops defer mode.
        { "defer_stop", key_queue::defer_stop },

// fw.defer_is_pending(slot_index: int, is_press: bool): bool
// Checks if a key press/release event is deferred on the given slot.
        { "defer_is_pending", key_queue::defer_is_pending },

// fw.defer_remove_last(): void
// Discards the most recent deferred event (if any).
        { "defer_remove_last", key_queue::defer_remove_last },

// fw.enter_bootloader(): void
// Reboots the system into the bootloader.
        { "enter_bootloader", fw_enter_bootloader },

// fw.execute_later(f, arg1, ...): void
// Schedules `f(arg1, ...)` to execute after all current key event processing completes.
        { "execute_later", execute_later },

// fw.keycode(keyname: string): int | void
// Returns the keycode (a.k.a. scan code) for the given keyname, which can be passed to
// fw.send_key(). Refer to hid_keycodes.hpp for valid key names.
        { "keycode", fw_keycode },

// fw.led0(): int
// Returns 1 if the debug LED is on, or 0 if it's off.
//
// fw.led0(x: int): void
// Turns the debug LED on (x=1), off (x=0), or toggle (x=-1).
        { "led0", fw_led0 },

// fw.led_refresh(): void
// Applies buffered color updates to the RGB LEDs. Changes are staged via
// fw.led_set_rgb() or fw.led_set_hsv().
        { "led_refresh", fw_led_refresh },

// fw.led_set_hsv(led_index: int, h: int, s: int, v: int): void
// Sets the HSV color of the RGB LED at the given led_index (also known as slot_index).
// Changes are buffered and take effect only after calling fw.led_refresh().
//
// HSV is well-suited for smooth color transitions and dynamic lighting:
//   - Rotating the Hue produces seamless rainbow animations.
//   - Adjusting the Value controls brightness without altering color tone.
// See hsv.hpp for details on HSV encoding, including valid ranges and hue sextant
// structure.
        { "led_set_hsv", fw_led_set_hsv },

// fw.led_set_rgb(led_index: int, r: int, g: int, b: int): void
// Sets the RGB color of the RGB LED at the given led_index (also known as slot_index).
// Changes are buffered and take effect only after calling fw.led_refresh().
        { "led_set_rgb", fw_led_set_rgb },

// fw.log(format: string, arg1, ...): void
// Lightweight printf-style logger optimized for embedded Lua environments, avoiding
// luaL_tolstring() to prevent temporary string allocations and reduce garbage
// collection overhead during frequent logging.
//
// Supported format specifiers are limited to '%d', '%s', and '%p', determined by
// each argument type:
//   - %d: integers, floats (converted to int), booleans (0/1), or nil (-1)
//   - %s: strings (passed as C strings)
//   - %p: tables, functions, or userdata (logged as raw pointers, e.g., 0xXXXX)
        { "log", fw_log },

// fw.log_mask(): int
// Returns the current log mask configured in the firmware.
//
// fw.log_mask(mask: int): void
// Sets the log mask to filter which subsystems emit logs.
//
// Log mask values (can be combined using bitwise OR):
//   - 1: Welcome message when starting `dalua`
//   - 2: Logs from usb_thread
//   - 4: Logs from matrix_thread
//   - 8: Logs from usbhub_thread
//   - 128: Logs from main_thread
        { "log_mask", fw_log_mask },

// fw.nvm: table (userdata, actually)
// Table of (name, value) pairs stored in NVM, persisting across reboots. Supported
// value types are int, float, and string. Assigning nil removes the entry from NVM.
// E.g. fw.nvm.foo = 1.2; fw.nvm.foo = "bar"; fw.nvm["foo"] = nil
        { "nvm", nullptr },  // placeholder for the `nvm` table

// fw.pack(...): table
// Equivalent to table.pack(); packs arguments into a table with a field 'n' for count.
        // { "pack", fw_pack },

// fw.unpack(t: table [, i: int [, j: int]]): (...)
// Equivalent to table.unpack(); returns the elements of the table from index i to j.
        // { "unpack", fw_unpack },

// fw.product_serial(): string
// Returns the product serial number inscribed in the USER page.
//
// fw.product_serial(s: string): void (triggers system reset)
// Writes the given product serial number into the USER page, but only if it is blank.
// Note that it cannot overwrite an existing product serial as it does not erase the
// USER page.
        { "product_serial", fw_product_serial },

// fw.send_key(keycode: int, is_press: bool): void
// Sends a key press or release event to the host.
// Note: No delay is needed between consecutive calls - timing is handled internally.
// You can safely call fw.send_key() back-to-back to send a press followed immediately
// by a release for the same key.
        { "send_key", fw_send_key },

// fw.stack_usage(): (string, ...)
// Returns a list of strings describing the stack usage of each thread.
// E.g.
//   ISR stack: 664 / 1024 bytes
//   main stack: 1292 / 2048 bytes
//   usbhub_thread stack: 552 / 1024 bytes
//   usbus stack: 932 / 1024 bytes
//   matrix_thread stack: 556 / 1024 bytes
        { "stack_usage", fw_stack_usage },

// fw.system_reset(): void
// Performs a system reset, rebooting the system.
        { "system_reset", fw_system_reset },

// fw.switchover(): void
// When both USB ports are connected to hosts, switches to the inactive host.
// Note: This triggers an immediate switchover and may leave residual key states in the
// previous host. See comments in usbus_hid_keyboard_t::on_reset() for details. It is
// OK to call from REPL, as REPL runs only when threads are idle. However, to safely call
// this from a keymap, wrap the call with fw.execute_later().
        { "switchover", fw_switchover },

// fw.timer_create(): userdata
// Creates and returns a timer instance.
// The `fw.timer_*()` functions support the Timer class implementation. See comments in
// core.lua for more details.
        { "timer_create", _timer_t::create },

// fw.timer_now(timer: userdata): int | void
// Returns the elapsed time since the epoch if the timer is active, or nothing otherwise.
        { "timer_now", _timer_t::now },

// fw.timer_start(timer: userdata, timeout_ms: int, callback [, repeated: bool]): int
// Starts or restarts the timer, setting its epoch to the current time. Returns 0 as the
// initial elapsed time since the epoch. `callback` is called when the timer expires
// later.
        { "timer_start", _timer_t::start },

// fw.timer_stop(timer: userdata): bool
// Stops the timer; returns true if the timer is active, or false if the timer was
// expired or never started.
        { "timer_stop", _timer_t::stop },

        { nullptr, nullptr }
    };

    luaL_newlib(L, fw_lib);
    (void)lua_newuserdata(L, 0);  // Create a new (full) userdata for the `nvm` object.

    lua_newtable(L);

// fw.nvm.__index(nvm, name: string): int | float | string | void
// Reads the value associated with the given key from NVM.
    lua_pushcfunction(L, _nvm_index);
    lua_setfield(L, -2, "__index");

// fw.nvm.__newindex(nvm, name: string, value): void
// Writes a (name, value) pair into NVM.
    lua_pushcfunction(L, _nvm_newindex);
    lua_setfield(L, -2, "__newindex");

// fw.nvm.__pairs(nvm): (iterator, nvm, nil)
// Returns an iterator and its initial arguments for traversing all entries in the `nvm`
// table.
    lua_pushcfunction(L, _nvm_pairs);
    lua_setfield(L, -2, "__pairs");

    lua_setmetatable(L, -2);

    lua_setfield(L, -2, "nvm");
    return 1;
}

}
