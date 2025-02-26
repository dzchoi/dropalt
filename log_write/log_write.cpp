#include "log.h"                // for LOG_ERROR, LOG_WARNING, ...
#include "log_module.h"
#include "mutex.h"

#include <cstdarg>              // for va_start(), va_end()
#include <cstdio>               // for fputs()
#include "backup_ram.hpp"       // for backup_ram::write()



static constexpr const char* COLOR_CODE[] = {
    "\e[0m",     // LOG_NONE (not used)
    "\e[0;31m",  // LOG_ERROR (red)
    "\e[0;33m",  // LOG_WARNING (yellow)
    "\e[0;36m",  // LOG_INFO (cyan)
    "\e[0m"      // LOG_DEBUG
};

void log_write_backup(unsigned level, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    static mutex_t log_lock = MUTEX_INIT;
    mutex_lock(&log_lock);  // It's safe to call even in an interrupt context.
    static unsigned last_level = LOG_DEBUG;
    if ( level != last_level ) {
        std::fputs(COLOR_CODE[level], stdout);
        last_level = level;
    }

    // Backup and print.
    std::fputs(backup_ram::write(format, args), stdout);
    mutex_unlock(&log_lock);
    va_end(args);
}
