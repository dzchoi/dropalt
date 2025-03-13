// Embedded Lua module capable of executing Lua bytecode from NVM and the REPL.

#pragma once

#include "log.h"



// Print an error message in printf() fashion, implicitly adding "remote:" at the
// beginning and a newline at the end. The format doesn't need to be a literal string
// if it is the only argument.
#define l_message(format, ...) \
    _l_message ## __VA_OPT__(_v) (format __VA_OPT__(,) __VA_ARGS__)

#define _l_message(s)  LOG_ERROR("remote: %s\n", (s))
#define _l_message_v(format, ...)  LOG_ERROR("remote: " format "\n", __VA_ARGS__)



namespace lua {

using status_t = int;

void init();

// bool signal_key_event(size_t index, bool is_press, uint32_t timeout_us);

// void repl_run();
// void repl_quit();

}
