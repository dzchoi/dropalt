#include "log.h"                // for LOG_ERROR, LOG_WARNING and LOG_INFO
#include "log_module.h"
#include "mutex.h"

#include <cstdarg>              // for va_start(), va_end()
#include <cstdio>               // for stdout, fputs(), vprintf()
// #include "persistent.hpp"       // for persistent::add_log()



void log_write_safe(unsigned level, const char* format, ...)
{
    static mutex_t log_mutex = MUTEX_INIT;
    mutex_lock(&log_mutex);

    va_list args1;
    va_start(args1, format);

    const char* font_color = nullptr;
    switch ( level ) {
        case LOG_ERROR:
            // va_list args2;
            // va_copy(args2, args1);
            // persistent::add_log(format, args2);
            // va_end(args2);

            font_color = "\e[0;31m";  // red
            break;

        case LOG_WARNING:
            font_color = "\e[0;33m";  // yellow
            break;

        case LOG_INFO:
            font_color = "\e[0;36m";  // cyan
            break;
    }

    if ( font_color )
        fputs(font_color, stdout);

    vprintf(format, args1);
    va_end(args1);

    if ( font_color )
        fputs("\e[0m", stdout);

    mutex_unlock(&log_mutex);
}
