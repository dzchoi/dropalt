// Custom log_module.h automatically included in log.h.

#pragma once

#include <stdarg.h>             // for va_list
#include <stdint.h>             // for uint8_t

#include "log.h"                // for LOG_NONE



// In addition to the global LOG_LEVEL we can define LOCAL_LOG_LEVEL, so only those LOG_*
// functions will be generated whose level is <= min(LOG_LEVEL, LOCAL_LOG_LEVEL).
#ifndef LOCAL_LOG_LEVEL
#define LOCAL_LOG_LEVEL LOG_ALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Core function for LOG_*(). It displays the log (if CDC ACM is connected) and stores it
// in the backup ram.
void log_backup(unsigned level, const char* format, ...);

// va_list variant of log_backup().
void vlog_backup(unsigned level, const char* format, va_list args);

uint_fast8_t get_log_mask(void);

void set_log_mask(uint_fast8_t mask);

// Each bit in the mask toggles the logging for the thread associated with its priority
// level (1 << thread_get_priority()). The LSB (bit 0) is unrelated to masking logs, and
// it only controls the displaying of the Lua welcome message. See lua::repl::start().
static const uint_fast8_t LOG_MASK_WELCOME = 0x01;

// LOG_LUA_ERROR is assigned LOG_NONE, which can now be used to indicate Lua error logs.
// Note that LOG_LUA_ERROR is not affected by "#define LOG_LEVEL LOG_NONE" and will
// always be displayed.
static const unsigned LOG_LUA_ERROR = LOG_NONE;

#ifdef __cplusplus
}  // extern "C"

// Customized log_write() for LOG_* functions. See riot/core/lib/include/log.h.
#   define log_write(level, ...)  do { \
    if constexpr ( (level) <= LOCAL_LOG_LEVEL ) log_backup((level), __VA_ARGS__); \
    } while (0U)

    // We could define log_write() as an inline function in C++ instead of a macro, but
    // this header file is included within `extern "C" {` in log.h.
    // extern "C" void log_backup(unsigned level, const char* format, ...);
    // template <typename... Ts>
    // inline void log_write(unsigned level, Ts... args) {
    //     if constexpr ( level <= LOCAL_LOG_LEVEL )
    //         log_backup(level, args...);
    // }
#else
#   define log_write(level, ...)  do { \
    if ( (level) <= LOCAL_LOG_LEVEL ) log_backup((level), __VA_ARGS__); } while (0U)
#endif
