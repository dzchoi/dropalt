#include <stdarg.h>             // for va_start(), va_end()
#include <stdio.h>              // for vsnprintf()
#include "assert.h"
#include "backup_ram.h"
#include "board.h"              // for _sheap1, _eheap1
#include "compiler_hints.h"     // for likely()



// These static variables are allocated in backup ram at fixed addresses, allowing them
// to be shared between the bootloader and the firmware.

// *pwrite_offset indicates the offset in the buffer where the next write will begin.
static uint16_t* const pwrite_offset = (uint16_t*)&_sheap1;

// *psize_limited indicates the portion of the buffer limited by the size constraint due
// to overflow.
static uint16_t* const psize_limited = (uint16_t*)(&_sheap1) + 1;

// Buffer that holds the concatenated big string.
static char* const buffer = (char*)(&_sheap1) + 2* sizeof(uint16_t);
static const size_t BUFFER_SIZE = (BACKUP_RAM_LEN / 2) - 2* sizeof(uint16_t);

// Scratchpad memory used for defragmenting the buffer.
static char* const scratchpad = (char*)(&_sheap1) + (BACKUP_RAM_LEN / 2);



#pragma GCC diagnostic ignored "-Warray-bounds"

void backup_ram_init(void)
{
    *pwrite_offset = 0;
    *psize_limited = 0;

    // Safe guard for backup_ram_read() to not return a too long C-string.
    buffer[BUFFER_SIZE - 1] = 0;

    // Ensure no other variables are allocated in backup ram.
    // Note that _eheap1 is determined at link time and cannot be known at compile-time.
    if ( (void*)&_eheap1 - (void*)&_sheap1 != BACKUP_RAM_LEN )
        // We cannot use assert() at this early stage of booting.
        while (1) {}
}

const char* backup_ram_write(const char* format, va_list vlist)
{
    va_list vlist_copy;
    va_copy(vlist_copy, vlist);

    uint16_t old_offset = *pwrite_offset;

    // Note: vsnprintf() returns the total number of characters (excluding the
    // terminating null byte) which was written successfully or would have been written
    // if not limited by the buffer size.
    int n = vsnprintf(
        buffer + old_offset, BUFFER_SIZE - old_offset, format, vlist);
    assert( n >= 0 && (size_t)n < BUFFER_SIZE );  // n excludes the null terminator.

    if ( likely(old_offset + (size_t)n < BUFFER_SIZE) )
        *pwrite_offset += n;
    else {
        // The buffer overflowed. We are overwriting it from the beginning now.
        // Note that m_size_limited will always be > 0 once an overflow occurs, since
        // vsnprintf() returns the length of written string, excluding the null.
        *psize_limited = old_offset;
        old_offset = 0;
        *pwrite_offset = vsnprintf(buffer, BUFFER_SIZE, format, vlist_copy);
    }

    va_end(vlist_copy);
    return buffer + old_offset;
}

const char* backup_ram_read(void)
{
    // Remaining string that was not overwritten
    const uint16_t remainder = *pwrite_offset + 1;  // Include the null terminator.

    // Defragment the buffer.
    const uint16_t size_limited = *psize_limited;
    if ( remainder < size_limited ) {  // Also checks if size_limited > 0.
        __builtin_memcpy(scratchpad, buffer, remainder);
        __builtin_memmove(buffer, buffer + remainder, size_limited - remainder);
        __builtin_memcpy(buffer + size_limited - remainder, scratchpad, remainder);
        *pwrite_offset = size_limited - 1;
        *psize_limited = 0;
    }

    return buffer;
}
