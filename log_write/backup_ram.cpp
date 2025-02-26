#include "assert.h"
#include "compiler_hints.h"     // for likely()

#include <cstdarg>              // for va_start(), va_end()
#include <cstdio>               // for vsnprintf()
#include "backup_ram.hpp"



namespace backup_ram {

// Offset in the buffer where the next write will begin.
#ifdef RIOTBOOT
// Allocate it in the non-initialized section (".heap3") of the backup ram, so it will
// persist through resets.
static uint16_t m_write_offset __attribute__((section(".heap3")));
#else
// ".backup.bss" will initialize the data to 0. See how reset_handler_default() zeroes
// out the memory region starting at _sbackup_bss.
static uint16_t m_write_offset __attribute__((section(".backup.bss")));
#endif

// Amount of the buffer constrained by the size limitation due to overflow.
#ifdef RIOTBOOT
static uint16_t m_size_limited __attribute__((section(".heap3")));
#else
static uint16_t m_size_limited __attribute__((section(".backup.bss")));
#endif

// Buffer that holds the concatenated big string.
static constexpr size_t BUFFER_SIZE = (BACKUP_RAM_LEN / 2) - 2* sizeof(uint16_t);
static char m_buffer[BUFFER_SIZE] __attribute__((section(".heap3")));

// Scratchpad memory used for defragmenting the buffer.
static constexpr size_t SCRATCHPAD_SIZE = BACKUP_RAM_LEN / 2;
static char m_scratchpad[SCRATCHPAD_SIZE] __attribute__((section(".heap3")));

// Note: the linker would cause an error if the variables declared above do not fit in
// the backup ram. E.g. "region `bkup_ram' overflowed by 4 bytes"



const char* write(const char* format, va_list vlist)
{
    va_list vlist_copy;
    va_copy(vlist_copy, vlist);

    // Note: vsnprintf() returns the total number of characters (excluding the
    // terminating null byte) which was written successfully or would have been written
    // if not limited by the buffer size.
    int n = std::vsnprintf(
        m_buffer + m_write_offset, sizeof(m_buffer) - m_write_offset, format, vlist);
    assert( n >= 0 && size_t(n) < sizeof(m_buffer) );  // n excludes the null terminator.

    if ( likely(m_write_offset + size_t(n) < sizeof(m_buffer)) )
        m_write_offset += n;
    else {
        // The buffer overflowed. We are overwriting it from the beginning now.
        // Note that m_size_limited will always be > 0 once an overflow occurs, since
        // vsnprintf() returns the length of written string, excluding the null.
        m_size_limited = m_write_offset;
        m_write_offset = std::vsnprintf(m_buffer, sizeof(m_buffer), format, vlist_copy);
        n = m_write_offset;
    }

    va_end(vlist_copy);
    return m_buffer + m_write_offset - n;
}

const char* read()
{
    // Remaining string that was not overwritten
    const uint8_t remainder = m_write_offset + 1;  // Include the null terminator.

    // Defragment the buffer.
    if ( remainder < m_size_limited ) {  // Also checks if size_limited > 0.
        __builtin_memcpy(m_scratchpad, m_buffer, remainder);
        __builtin_memmove(m_buffer, m_buffer + remainder, m_size_limited - remainder);
        __builtin_memcpy(m_buffer + m_size_limited - remainder, m_scratchpad, remainder);
        m_write_offset = m_size_limited;
        m_size_limited = 0;
    }

    return m_buffer;
}

}
