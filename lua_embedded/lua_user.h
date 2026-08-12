#pragma once

/*
 * Lua's default lua_writestring() implementation uses fwrite(stdout).  Newlib
 * allocates its FILE objects on first use, which is unsafe when Lua has already
 * consumed nearly all of the system heap.  RIOT's stdio_write() goes directly
 * to the configured stdio backend and does not need that initialization.
 */
#include "stdio_base.h"

#ifdef __cplusplus
extern "C" {
#endif

size_t lua_user_number2str(char *buffer, size_t size, float value);

#ifdef __cplusplus
}
#endif

/* Lua is configured with 32-bit floats. Avoid newlib's 6-KiB floating printf
 * implementation when tostring() and the REPL display a number. */
#undef lua_number2str
#define lua_number2str(buffer, size, value) \
    lua_user_number2str((buffer), (size), (float)(value))

#define lua_writestring(string, length) \
    ((void)stdio_write((string), (length)))

#define lua_writeline() \
    lua_writestring("\n", 1)

/* All Lua 5.3 callers pass a format containing one "%s" conversion. */
static inline void lua_user_writestringerror(const char *format,
                                             const char *parameter)
{
    const char *literal = format;
    const char *cursor = format;

    while (*cursor != '\0') {
        if (cursor[0] == '%' && cursor[1] == 's') {
            lua_writestring(literal, (size_t)(cursor - literal));
            const char *parameter_end = parameter;
            while (*parameter_end != '\0') {
                ++parameter_end;
            }
            lua_writestring(parameter,
                            (size_t)(parameter_end - parameter));
            cursor += 2;
            literal = cursor;
            continue;
        }
        ++cursor;
    }

    lua_writestring(literal, (size_t)(cursor - literal));
}

#define lua_writestringerror(format, parameter) \
    lua_user_writestringerror((format), (parameter))
