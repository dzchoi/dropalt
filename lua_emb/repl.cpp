#include "assert.h"
#include "log.h"                // for get_log_mask()
#include "stdio_base.h"         // for stdio_write()

#include "config.hpp"           // for ENABLE_LUA_REPL
#include "lua.hpp"              // for LUA_COPYRIGHT, l_message(), ...
#include "repl.hpp"
#include "timed_stdin.hpp"      // for timed_stdin::_reader()

static_assert( ENABLE_LUA_REPL );



namespace lua {

void repl::start()
{
    respond(LUA_OK);  // Indicate that REPL is ready.

    if ( get_log_mask() & LOG_MASK_WELCOME ) {
        constexpr char welcome_msg[] = LUA_COPYRIGHT "\n";
        // Then output the welcome message.
        stdio_write(welcome_msg, __builtin_strlen(welcome_msg));
    }
}

void repl::report(status_t status)
{
    global_lua_state L;

    if ( status == LUA_OK ) {
        for ( int i = -lua_gettop(L) ; i < 0 ; i++ ) {
            size_t l;
            const char* s = luaL_tolstring(L, i, &l);
            lua_writestring(s, l);
            lua_writeline();
            lua_pop(L, 1);
        }
        lua_settop(L, 0);  // Remove all values from the stack.

        // Alternatively, we could use `print` (i.e. luaB_print) from base library.
        // int n = lua_gettop(L);
        // if ( n > 0 ) {  // Any result to print?
        //     // luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
        //     lua_getglobal(L, "print");
        //     lua_insert(L, 1);
        //     status = lua_pcall(L, n, 0, 0);
        // }
    }
    else {
        // Since we load a pre-compiled chunk from the host, encountering a LUA_ERRSYNTAX
        // error is typically unlikely. However, if the received chunk is not valid,
        // LUA_ERRSYNTAX might occur, although this will not happen with our (DTE-aware)
        // timed_stdin.
        l_message(lua_tostring(L, -1));
        lua_pop(L, 1);  // remove the error message.
    }
}

void repl::respond(status_t status)
{
    // As the host operates in canonical input mode, binary status (e.g., '\x4') cannot
    // be sent directly. Convert the status into a printable ASCII character instead.
    const ping resp(status);
    stdio_write(&resp, sizeof(resp));
}

void repl::execute()
{
    global_lua_state L;
    LOG_DEBUG("Lua: repl::execute()");

    // We use a timed reader function here instead of stdio_read() from stdio_base.h.
    // This ensures that if the host's serial terminal (e.g., dalua) is malfunctioning
    // and fails to provide the expected input, the REPL does not become unresponsive.
    // Note: We use 100 ms here for the timeout to wait for receiving subsequent chunks.
    status_t status = lua_load(L, timed_stdin::_reader, (void*)100, nullptr, "b");
    if ( status == LUA_OK )
        // If an error occurs in lua_pcall(), the error message will include the progname
        // specified by luaL_loadbuffer() from the host, provided lua_dump() has not
        // stripped debug information. Otherwise, the progname will be shown as "-1".
        status = lua_pcall(L, 0, LUA_MULTRET, 0);

    report(status);   // Show the result or error.
    respond(status);  // Respond to the host with the status.
}

status_t ping::status() const
{
    if ( hd0 == '[' && hd1 == '}' && nl == '\n' )
        return st - '0';
    return LUA_NOSTATUS;
}

}
