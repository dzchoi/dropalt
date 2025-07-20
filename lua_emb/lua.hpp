// Embedded Lua module capable of executing Lua bytecode from NVM and the REPL.

#pragma once

#include <climits>              // for INT_MIN
#include <cstdint>              // for uint32_t, int8_t

#include "log.h"

extern "C" {
// #include "lua.h"
#include "lauxlib.h"            // declares luaL_*().
}



// Output an error message in printf() fashion, automatically appending a newline
// at the end. If the format is the only argument, it doesn't need to be a literal
// string.
#define l_message(format, ...) \
    _l_message ## __VA_OPT__(_v) (format __VA_OPT__(,) __VA_ARGS__)

#define _l_message(s)  LOG(LOG_LUA_ERROR, "%s\n", (s))
#define _l_message_v(format, ...)  LOG(LOG_LUA_ERROR, format "\n", __VA_ARGS__)



constexpr int LUA_NOSTATUS = INT_MIN;

namespace lua {

using status_t = int;

// Shared global Lua state used by both the keymap and REPL modules.
inline lua_State* L = nullptr;

void init();

class ping {
public:
    constexpr ping(status_t status):
        hd0('['), hd1('}'), st(status + '0'), nl('\n') {}

    operator uint32_t() const { return value; }

    // Retrieve the status embedded in the ping, or return -1 if the ping is invalid.
    status_t status() const;

private:
    union {
        uint32_t value;
        struct {
            const char hd0;   // '['
            const char hd1;   // '}'
            const int8_t st;  // 1 byte of status
            const char nl;    // '\n'
        } __attribute__((packed));
    };
};

}
