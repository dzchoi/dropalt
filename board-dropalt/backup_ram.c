#include <stdint.h>

#include "assert.h"
#include "backup_ram.h"



// `no_reorder` keeps backup_t and the user buffer at fixed addresses under LTO. Neither
// object is initialized by the C runtime.
struct backup_t backup
    __attribute__((section(".backup.noinit"), used, no_reorder));

// This array intentionally consumes all backup RAM remaining after `backup`. Any other
// object placed in `.backup.noinit` will therefore overflow the region at link time. It
// also leaves RIOT's secondary heap empty (`_sheap1 == _eheap1`), so `_sbrk_r()` cannot
// allocate from backup RAM even though `sam0_common` configures `NUM_HEAPS` as 2.
uint8_t backup_ram_buffer[BACKUP_RAM_BUFFER_LEN]
    __attribute__((section(".backup.noinit"), used, no_reorder, aligned(4)));

static_assert(sizeof(backup) + sizeof(backup_ram_buffer) == BACKUP_RAM_LEN,
              "backup RAM allocation has the wrong size");

void backup_ram_init(void)
{
    // __builtin_memset(&backup, 0, sizeof(backup));
    // __builtin_memset(backup_ram_buffer, 0, sizeof(backup_ram_buffer));
    extern uint8_t _sbackup_noinit[];
    extern uint8_t _ebackup_noinit[];
    __builtin_memset(_sbackup_noinit, 0, _ebackup_noinit - _sbackup_noinit);
}
