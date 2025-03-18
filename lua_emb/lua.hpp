// Embedded Lua module capable of executing Lua bytecode from NVM and the REPL.

#pragma once

#include "log.h"



// Output an error message in printf() fashion, automatically appending a newline
// at the end. If the format is the only argument, it doesn't need to be a literal
// string.
#define l_message(format, ...) \
    _l_message ## __VA_OPT__(_v) (format __VA_OPT__(,) __VA_ARGS__)

#define _l_message(s)  LOG_ERROR("%s\n", (s))
#define _l_message_v(format, ...)  LOG_ERROR(format "\n", __VA_ARGS__)



struct lua_State;

namespace lua {

using status_t = int;

int luaopen_fw(lua_State* L);

void init();

// void repl_run();
// void repl_quit();

}
