#pragma once

#include <stdarg.h>             // va_list
#include <stdint.h>             // for uint8_t, uint16_t



#ifdef __cplusplus
extern "C" {
#endif

// The members of backup_t are allocated in backup RAM at fixed addresses, allowing them
// to be shared between different firmware versions. Note: Do not reorder members.

extern struct backup_t {
    // Indicates the offset in log_buffer where the next write will begin.
    uint16_t write_offset;

    // Indicates the end of log_buffer where the buffer is rolled over due to overflow.
    uint16_t size_limited;

    // Tracks the currently active USB host port.
    uint8_t current_host_port;

    // Add new members here.
} backup;

void backup_ram_init(void);

// Build a string from the given parameters, store it in the backup ram, and return
// it as a C-string.
const char* backup_ram_write(const char* format, va_list args);

// Return the stored log strings as a single concatenated C-string.
const char* backup_ram_read(void);

#ifdef __cplusplus
}
#endif
