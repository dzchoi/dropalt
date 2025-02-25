#include "log.h"                // for LOG_ERROR, LOG_WARNING, ...
#include "log_module.h"
#include "mutex.h"

#include <cstdarg>              // for va_start(), va_end()
#include <cstdio>               // for fputs(), vprintf()



static constexpr const char* COLOR_CODE[] = {
    "\e[0m",     // LOG_NONE (not used)
    "\e[0;31m",  // LOG_ERROR (red)
    "\e[0;33m",  // LOG_WARNING (yellow)
    "\e[0;36m",  // LOG_INFO (cyan)
    "\e[0m"      // LOG_DEBUG
};

void log_write_safe(unsigned level, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    static mutex_t coloring_lock = MUTEX_INIT;
    mutex_lock(&coloring_lock);  // Safe to call even in an interrupt context.
    static unsigned last_level = LOG_DEBUG;
    if ( level != last_level ) {
        fputs(COLOR_CODE[level], stdout);
        last_level = level;
    }
    mutex_unlock(&coloring_lock);

    vprintf(format, args);
    va_end(args);
}
