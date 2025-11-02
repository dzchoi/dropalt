// Embedded Lua module capable of executing Lua bytecode from NVM and the REPL.

#pragma once

#include "log.h"

extern "C" {
// #include "lua.h"             // already included by lauxlib.h
#include "lauxlib.h"            // declares luaL_*().
}



// Output an error message in printf() fashion, automatically appending a newline
// at the end. If the format is the only argument, it doesn't need to be a literal
// string.
#define l_message(...) LOG(LOG_LUA_ERROR, __VA_ARGS__)



namespace lua {

// Two categories of Lua functions that operate on `L` (lua_State):
//
// [lua_CFunction]
// - Signature: `int (*)(lua_State*)`
// - Receives its own stack frame for passing arguments and returning results.
// - Upon return, the Lua interpreter (e.g. `lua_pcall()`) automatically discards stack
//   elements below the returned results; manual cleanup is not required.
// - Should be self-contained: validate arguments using `luaL_check*()` or
//   `luaL_argcheck()`, and use `lua_error()` instead of C `assert()` to report errors.
//
// [C-side Lua utilities]
// - Examples: lua_insert(), lua::load_keymap(), lua::repl::execute(), ...
// - The state of the stack below the expected arguments is undefined.
// - Therefore, only negative indices (relative to the top of the stack) should be used
//   to reference stack elements reliably.

// Initialize a global Lua state shared by both the keymap and REPL modules.
// local declaration of `global_lua_state L;` provides access to the shared Lua state
// using `L` and asserts that the Lua stack is empty when it goes out of scope.
// Note that though the state is accessed by multiple subsystems, including timer
// callbacks, all accesses are serialized to prevent concurrent use.
class global_lua_state {
public:
    ~global_lua_state();

    static void init();

    static bool validate_bytecode(uintptr_t addr);

    operator lua_State*() { return L; }

private:
    static inline lua_State* L = nullptr;
};

// Type alias for Lua status codes (e.g. LUA_OK) defined in lua.h.
using status_t = int;

}
