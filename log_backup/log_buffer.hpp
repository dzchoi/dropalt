// Ring buffer log manager supporting concurrent reads and writes.
//
// Logs are kept by append() as compact records containing a timestamp, format string,
// and arguments. read() renders them into ordinary text when needed, while the
// reset-safe ring buffer preserves completed records in backup RAM across a reset.
//
// * backup_ram_buffer: [format cache][record ring]
// * record_t: [header][encoded arguments][copied format / %s strings][alignment padding]
// * Record starts and sizes are 4-byte aligned. Unused tail bytes are represented by a
//   padding record (a record whose size is 0).

#pragma once

#include <cstdarg>              // for va_list
#include <cstddef>
#include <cstdint>
#include <utility>              // for std::pair<>

#include "backup_ram.h"

struct reader_state_t;



// Sentinel offset values.
constexpr offset_t READER_BEGIN = static_cast<offset_t>(-1u);
constexpr offset_t READER_END = static_cast<offset_t>(-2u);

constexpr size_t RECORD_ALIGNMENT = alignof(uint32_t);

inline constexpr char INVALIDATED_MARKER[] = "\n...\n";



class log_buffer {
public:
    // Offset of the first (oldest) valid log record (always < sizeof(m_buffer)).
    static offset_t begin() { return m_begin; }

    // Offset where the next log record will be written (always < sizeof(m_buffer)).
    static offset_t end() { return m_end; }

    static uint16_t& size(offset_t offset) {
        return *reinterpret_cast<uint16_t*>(
            __builtin_assume_aligned(&m_buffer[offset], alignof(uint16_t)));
    }

    // A padding record has its size of 0.
    static bool is_padding(offset_t offset) { return size(offset) == 0u; }

    // Checks whether the record at offset, with the given timestamp, is stale.
    static bool is_stale(offset_t offset, uint32_t timestamp);

    // Checks if the buffer is empty. It's empty iff begin() is a padding record.
    static bool empty() { return is_padding(begin()); }

    // `offset` is required to be a valid record offset.
    static offset_t next(offset_t offset);

    // Creates a record and appends it to the log buffer, possibly deleting old records.
    // Returns end() if it fails.
    static offset_t append(unsigned level, unsigned pid, uint32_t timestamp,
                           const char* format, va_list args);

    // Reads available retained records into the buffer. If a concurrent append()
    // overwrites unread records, inserts "\n...\n" and resumes at the oldest record.
    // Note: stale-record recovery requires size >= 5 for the "\n...\n" marker.
    static size_t read(reader_state_t& reader, char* buffer, size_t size);

private:
    friend class record_t;

    constexpr log_buffer() =delete;

    static constexpr size_t FORMAT_CACHE_SIZE = 640;
    static_assert( FORMAT_CACHE_SIZE < sizeof(backup_ram_buffer) );
    static_assert( FORMAT_CACHE_SIZE % RECORD_ALIGNMENT == 0,
                   "the record ring must start aligned");
    static_assert( BACKUP_RAM_BUFFER_LEN % RECORD_ALIGNMENT == 0,
                   "the record ring must have an aligned size");

    inline static uint8_t (&m_cache)[FORMAT_CACHE_SIZE] =
        reinterpret_cast<uint8_t(&)[FORMAT_CACHE_SIZE]>(backup_ram_buffer);

    inline static uint8_t (&m_buffer)[sizeof(backup_ram_buffer) - FORMAT_CACHE_SIZE] =
        *reinterpret_cast<uint8_t(*)[sizeof(backup_ram_buffer) - FORMAT_CACHE_SIZE]>
            (backup_ram_buffer + FORMAT_CACHE_SIZE);

    // Two offset values are reserved for reader-state sentinels.
    static_assert( sizeof(m_buffer) <= UINT16_MAX - 1u,
                   "record offsets must fit without using reader sentinels");

    // m_begin and m_end are offsets in m_buffer[].
    inline static offset_t& m_begin = backup.log_begin;
    inline static offset_t& m_end = backup.log_end;
};



// Lightweight, non-owning view of a packed record stored directly in the log buffer.
// Note: Callers must disable IRQs during use to prevent an interrupting append() from
// modifying or invalidating the underlying memory.
class record_t {
public:
    struct header_t {
        uint16_t  size;
        uint8_t   level;
        uint8_t   pid;  // KERNEL_PID_UNDEF .. KERNEL_PID_LAST, KERNEL_PID_ISR
        uint32_t  timestamp;
        uintptr_t format;
    };
    static_assert( sizeof(header_t) == 12 );
    static_assert( alignof(header_t) <= RECORD_ALIGNMENT );
    static_assert( sizeof(header_t) % RECORD_ALIGNMENT == 0 );

    uint16_t size() const { return m_pheader->size; }
    uint8_t level() const { return m_pheader->level; }
    uint8_t pid() const { return m_pheader->pid; }
    uint32_t timestamp() const { return m_pheader->timestamp; }
    uintptr_t format() const { return m_pheader->format; }
    va_list arguments() const;

    explicit record_t(offset_t offset)
    : m_offset(offset)
    , m_pheader(reinterpret_cast<header_t*>(
        __builtin_assume_aligned(&log_buffer::m_buffer[offset], alignof(header_t))))
    {}

    static std::pair<size_t, size_t> measure_sizes(const char* format, va_list args);

private:
    offset_t m_offset;
    header_t* const m_pheader;
};



constexpr bool supported_specifier(char specifier)
{
    return specifier == '%' || specifier == 'd' || specifier == 'u'
        || specifier == 'x' || specifier == 's' || specifier == 'p';
}

using output_f = void (*)(void* context, const char* data, size_t size);

void output_header(output_f output, void* context,
                   unsigned level, unsigned pid, uint32_t timestamp);

void output_format(output_f output, void* context, const char* format, va_list args);
