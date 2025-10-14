#include <stdarg.h>             // for va_start(), va_end()

#include "backup_ram.h"         // for backup_ram_write()
#include "irq.h"                // for irq_disable(), irq_restore()
#include "log.h"                // for LOG_ERROR, LOG_WARNING, ...
#include "log_module.h"
#include "stdio_base.h"         // for stdio_write()
#include "thread.h"             // for thread_get_active(), thread_get_priority()



static const char* COLOR_CODE[] = {
    "\e[0m",     // LOG_LUA_ERROR
    "\e[0;31m",  // LOG_ERROR (red)
    "\e[0;33m",  // LOG_WARNING (yellow)
    "\e[0;36m",  // LOG_INFO (cyan)
    "\e[0m"      // LOG_DEBUG
};

static uint8_t log_mask = 0xff;

void log_backup(unsigned level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vlog_backup(level, format, args);
    va_end(args);
}

void vlog_backup(unsigned level, const char* format, va_list args)
{
    // Defer context switching by disabling the PendSV interrupt. This ensures that
    // stdio_write() can safely push log strings into cdcacm->tsrb without being
    // preempted or interrupted, and that the cdcacm->flush events triggered by
    // stdio_write() are deferred and processed together only after irq_enable() is
    // called.
    unsigned state = irq_disable();

    // All logs are first saved to backup RAM, regardless of log_mask filtering.
    const char* line = backup_ram_write(format, args);

    // LOG_LUA_ERROR is always displayed, regardless of the current thread.
    if ( level == LOG_LUA_ERROR
      // All LOG_* messages are displayed during boot code (kernel_init() and main()).
      || unlikely(thread_get_active() == NULL)
      // Otherwise, LOG_* messages are filtered based on log_mask and thread priority.
      || (log_mask & (1 << thread_get_priority(thread_get_active()))) ) {
        const size_t len = __builtin_strlen(line);
        // assert( len > 0 && line[len - 1] == '\n' );
        switch ( level ) {
            case LOG_ERROR:
            case LOG_WARNING:
            case LOG_INFO:
                if ( len > 1 ) {  // If the line is not just "\n",
                    // Restore color before the trailing '\n'; In canonical-mode
                    // terminals, escape sequences after a newline will appear as part
                    // of the next line input.
                    stdio_write(COLOR_CODE[level], __builtin_strlen(COLOR_CODE[level]));
                    stdio_write(line, len - 1);
                    stdio_write(
                        COLOR_CODE[LOG_DEBUG], __builtin_strlen(COLOR_CODE[LOG_DEBUG]));
                    stdio_write("\n", 1);
                    break;
                }
                // Intentional fall-through

            default:
                stdio_write(line, len);
                break;
        }
    }

    irq_restore(state);
}

uint8_t get_log_mask(void)
{
    return log_mask;
}

void set_log_mask(uint8_t mask)
{
    unsigned state = irq_disable();
    log_mask = mask;
    irq_restore(state);
}
