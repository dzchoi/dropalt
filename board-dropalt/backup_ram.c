#include <stdarg.h>             // for va_start(), va_end()
#include <stdio.h>              // for vsnprintf()
#include "assert.h"
#include "backup_ram.h"
#include "compiler_hints.h"     // for likely()



// These static variables are allocated in backup ram at fixed addresses, allowing them
// to be shared between the bootloader and the firmware.

// `write_offset` indicates the offset in the buffer where the next write will begin.
// Note that "used" here is used to preserve the allocation order of variables in the
// section.
static uint16_t write_offset __attribute__((section(".backup.noinit"), used));

// `size_limited` indicates the portion of the buffer limited by the size constraint due
// to overflow.
static uint16_t size_limited __attribute__((section(".backup.noinit"), used));

// Buffer that holds the concatenated big string.
static char buffer[BACKUP_RAM_LEN - 2* sizeof(uint16_t)]
    __attribute__((section(".backup.noinit"), used));

// Note that if there is another variable allocated in the backup ram, the linker will
// detect an overflow. Since we are allocating the entire backup ram here, _sbrk_r() in
// riot/sys/newlib_syscalls_default/syscalls.c will not configure a heap on this region
// even if NUM_HEAPS is set to 2 in riot/cpu/sam0_common/include/cpu_conf.h.

void backup_ram_init(void)
{
    write_offset = 0;
    size_limited = 0;

    // Safe guard for backup_ram_read() to not return a too long C-string.
    buffer[sizeof(buffer) - 1] = 0;
}

const char* backup_ram_write(const char* format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);

    // Note: vsnprintf() returns the total number of characters (excluding the
    // terminating null byte) which was written successfully or would have been written
    // if not limited by the buffer size.
    int n = vsnprintf(
        buffer + write_offset, sizeof(buffer) - write_offset, format, args);
    assert( n >= 0 && (size_t)n < sizeof(buffer) );  // n excludes the null terminator.

    if ( likely(write_offset + (size_t)n < sizeof(buffer)) )
        write_offset += n;
    else {
        // The buffer overflowed. We are overwriting it from the beginning now.
        // Note that size_limited will get > 0 below, indicating an overflow has occurred,
        // since the previous vsnprintf() would have written at least one non-overflowing
        // string into the buffer.

        // Note that this second vsnprintf() should not overflow the buffer, as it was
        // already verified at the first vsnprintf() call.
        n = vsnprintf(buffer, sizeof(buffer), format, args_copy);
        size_limited = write_offset;
        write_offset = n;
    }

    va_end(args_copy);
    return buffer + (write_offset - n);
}

static void reverse_memory(char* begin, char* end)
{
    for ( ; begin < --end ; ++begin ) {
        const char temp = *begin;
        *begin = *end;
        *end = temp;
    }
}

const char* backup_ram_read(void)
{
    // Remaining string that was not overwritten
    const uint16_t remainder = write_offset + 1;  // Include the null terminator.

    // Defragment the buffer.
    if ( remainder < size_limited ) {  // Also checks if size_limited > 0.
        reverse_memory(buffer, buffer + remainder);
        reverse_memory(buffer + remainder, buffer + size_limited);
        reverse_memory(buffer, buffer + size_limited);
        write_offset = size_limited - 1;
        size_limited = 0;
    }

    return buffer;
}
