#pragma once

#include <stdarg.h>             // va_list



#ifdef __cplusplus
extern "C" {
#endif

void backup_ram_init(void);

// Build a string from the given parameters, store it in the backup ram, and return
// it as a C-string.
const char* backup_ram_write(const char* format, va_list args);

// Return the stored strings as a single concatenated C-string.
const char* backup_ram_read(void);

#ifdef __cplusplus
}
#endif
