#include <stdarg.h>             // for va_start(), va_end()
#include <stdio.h>              // for fputs()

#include "backup_ram.h"         // for backup_ram_write()
#include "log.h"                // for LOG_ERROR, LOG_WARNING, ...
#include "log_module.h"
#include "mutex.h"              // for mutex_lock(), mutex_unlock(), MUTEX_INIT
#include "thread.h"             // for thread_get_active(), thread_get_priority()



static const char* COLOR_CODE[] = {
    "\e[0m",     // LOG_LUA_ERROR
    "\e[0;31m",  // LOG_ERROR (red)
    "\e[0;33m",  // LOG_WARNING (yellow)
    "\e[0;36m",  // LOG_INFO (cyan)
    "\e[0m"      // LOG_DEBUG
};

static mutex_t log_lock = MUTEX_INIT;

static uint8_t log_mask = 0xff;

// Core function for LOG_*(). It displays the log (if CDC ACM is connected) and stores it
// in the backup ram.
void log_backup(unsigned level, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    mutex_lock(&log_lock);  // It's safe to call even in an interrupt context.
    // Even if a log is masked, we still store it in the backup ram.
    const char* log = backup_ram_write(format, args);
    if ( level == LOG_LUA_ERROR  // LOG_LUA_ERROR is displayed regardless of the thread.
      || (log_mask & (1 << thread_get_priority(thread_get_active()))) ) {
        fputs(COLOR_CODE[level], stdout);
        fputs(log, stdout);  // Backup and print.
        if ( level != LOG_DEBUG )
            fputs(COLOR_CODE[LOG_DEBUG], stdout);
    }
    mutex_unlock(&log_lock);

    va_end(args);
}

uint8_t get_log_mask(void)
{
    return log_mask;
}

void set_log_mask(uint8_t mask)
{
    mutex_lock(&log_lock);
    log_mask = mask;
    mutex_unlock(&log_lock);
}
