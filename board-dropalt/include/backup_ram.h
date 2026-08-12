#pragma once

#include <stdint.h>



#ifdef __cplusplus
extern "C" {
#endif

// The members of backup_t are allocated in backup RAM at fixed addresses, allowing them
// to be shared between resets.
extern struct backup_t {
    // Byte after the newest retained log record. (For a non-empty ring, this is updated
    // after a record is committed. For an empty ring, the record size is the commit
    // marker, so a reset during its first write still leaves the ring empty.)
    uint16_t log_end;

    // Offset of the oldest retained log record.
    uint16_t log_begin;

    // Byte size of the format string cache.
    uint16_t format_cache_size;

    // Tracks the currently active USB host port.
    uint8_t current_host_port;

    // Add new members as necessary.
} backup;

enum { BACKUP_RAM_BUFFER_LEN = BACKUP_RAM_LEN - sizeof(struct backup_t) };

// Space remaining after the fixed retained fields above. Modules may partition this
// region and own the contents of their partition.
extern uint8_t backup_ram_buffer[BACKUP_RAM_BUFFER_LEN] __attribute__((aligned(4)));

// Clear the entire backup RAM including the members of backup_t.
void backup_ram_init(void);

#ifdef __cplusplus
}
#endif
