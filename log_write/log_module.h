// Custom log_module.h included in log.h.

#pragma once

// In addition to the global LOG_LEVEL we can define LOCAL_LOG_LEVEL, so only those LOG_*
// functions will be generated whose level is <= min(LOG_LEVEL, LOCAL_LOG_LEVEL).
#ifndef LOCAL_LOG_LEVEL
#define LOCAL_LOG_LEVEL LOG_ALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

void log_write_backup(unsigned level, const char* format, ...);

// Customized log_write() for LOG_* functions. See riot/core/lib/include/log.h.
#ifdef __cplusplus
#   define log_write(level, ...)  do { \
    if constexpr ( (level) <= LOCAL_LOG_LEVEL ) log_write_backup((level), __VA_ARGS__); \
    } while (0U)

    // We could define log_write() as an inline function in C++ instead of a macro, but
    // this header file is included within `extern "C" {` in log.h.
    // template <typename... Ts>
    // inline void log_write(unsigned level, Ts... args) {
    //     if constexpr ( level <= LOCAL_LOG_LEVEL )
    //         log_write_backup(level, args...);
    // }
#else
#   define log_write(level, ...)  do { \
    if ( (level) <= LOCAL_LOG_LEVEL ) log_write_backup((level), __VA_ARGS__); } while (0U)
#endif

#ifdef __cplusplus
}
#endif
