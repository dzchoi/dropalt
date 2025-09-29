#pragma once

#include <climits>              // for INT_MIN
#include <cstdint>              // for uint32_t, int8_t

#include "lua.hpp"              // for status_t



// Additional Lua status returned by ping.
constexpr int LUA_NOSTATUS = INT_MIN;

namespace lua {

// This static class provides Lua REPL functionality.
// Note: This class is included in the build only when ENABLE_LUA_REPL is true.
class repl {
public:
    // Start the Lua REPL, enabling execution of Lua code received from stdin.
    static void start();

    // No-op. Ideally, this would revert all changes made to L across REPL sessions.
    static void stop() {}

    // Execute each pre-compiled Lua code chunk received from the host.
    // ( -- )
    static void execute();

private:
    constexpr repl() =delete;  // Ensure a static class

    // Print all values on the Lua stack if `status` is LUA_OK. Otherwise, print the
    // error message at the top of the stack, assuming it is a string produced by the
    // Lua interpreter or a custom 'msghandler'.
    // ( ... -- )
    static void report(status_t status);

    // Respond to the host with the execution status.
    static void respond(status_t status);
};

class ping {
public:
    constexpr ping(status_t status):
        hd0('['), hd1('}'), st(status + '0'), nl('\n') {}

    operator uint32_t() const { return uint32; }

    // Retrieve the status embedded in the ping, or return LUA_NOSTATUS if the ping is
    // invalid.
    status_t status() const;

private:
    union {
        uint32_t uint32;
        struct __attribute__((packed)) {
            const char hd0;   // '['
            const char hd1;   // '}'
            const int8_t st;  // 1 byte of status
            const char nl;    // '\n'
        };
    };
};

}
