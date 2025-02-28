#include <stdarg.h>             // for va_start(), va_end()
#include <stdio.h>              // for fputs()
#include "backup_ram.h"         // for backup_ram_write()
#include "log.h"                // for LOG_ERROR, LOG_WARNING, ...
#include "log_module.h"
#include "mutex.h"



static const char* COLOR_CODE[] = {
    "\e[0m",     // LOG_NONE (not used)
    "\e[0;31m",  // LOG_ERROR (red)
    "\e[0;33m",  // LOG_WARNING (yellow)
    "\e[0;36m",  // LOG_INFO (cyan)
    "\e[0m"      // LOG_DEBUG
};

// Core function for LOG_*(). It displays the log (if CDC ACM is connected) and stores it
// in the backup ram.
void log_backup(unsigned level, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    static mutex_t log_lock = MUTEX_INIT;
    mutex_lock(&log_lock);  // It's safe to call even in an interrupt context.
    static unsigned last_level = LOG_DEBUG;
    if ( level != last_level ) {
        fputs(COLOR_CODE[level], stdout);
        last_level = level;
    }

    // Backup and print.
    fputs(backup_ram_write(format, args), stdout);
    mutex_unlock(&log_lock);
    va_end(args);
}
