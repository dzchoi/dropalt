#include <algorithm>            // for std::min<>
#include <cstdarg>              // for va_copy(), va_end()
#include <tuple>                // for std::tie()

#include "assert.h"
#include "cpu_conf.h"
#include "irq.hpp"              // for riot::irq_lock
#include "log_reader.h"

#include "log_buffer.hpp"



namespace {

size_t align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

bool supported_format(const char* format)
{
    while ( *format ) {
        if ( *format++ == '%' && !supported_specifier(*format++) )
            return false;
    }
    return true;
}

bool is_persistent(const char* string)
{
    uintptr_t address = reinterpret_cast<uintptr_t>(string);
    uintptr_t cache = reinterpret_cast<uintptr_t>(backup_ram_buffer);
    return address < CPU_RAM_BASE
        || (address >= cache && address < cache + 640);
}

const char* cache_format(const char* format)
{
    if ( is_persistent(format) )
        return format;

    size_t length = __builtin_strlen(format) + 1;
    if ( length > UINT8_MAX )
        return format;

    size_t entry_size = sizeof(uint8_t) + length;
    uint16_t end = backup.format_cache_size;
    uint8_t* cache = backup_ram_buffer;
    uint16_t cursor = 0;
    while ( cursor < end ) {
        uint8_t cached_length = cache[cursor];
        size_t cached_size = sizeof(uint8_t) + cached_length;
        if ( cached_length == 0 || cached_size > static_cast<size_t>(end - cursor) )
            return format;
        const char* cached = reinterpret_cast<const char*>(cache + cursor + 1);
        if ( cached_length == length && __builtin_memcmp(cached, format, length) == 0 )
            return cached;
        cursor += cached_size;
    }
    if ( entry_size > static_cast<size_t>(640u - end) )
        return format;

    cache[end] = static_cast<uint8_t>(length);
    char* cached = reinterpret_cast<char*>(cache + end + 1);
    __builtin_memcpy(cached, format, length);
    __asm__ volatile("" ::: "memory");
    backup.format_cache_size = end + entry_size;
    return cached;
}

void copy_arguments(uint8_t* buffer, offset_t record_offset, offset_t& string_offset,
                    const char* format, va_list args)
{
    offset_t arg_offset = record_offset + sizeof(record_t::header_t);
    va_list copy;
    va_copy(copy, args);
    for ( const char* p = format ; *p ; ) {
        if ( *p++ != '%' )
            continue;

        switch ( *p++ ) {
            case '%':
                continue;

            case 'd': {
                uintptr_t arg = va_arg(copy, int);
                __builtin_memcpy(buffer + arg_offset, &arg, sizeof(arg));
                arg_offset += sizeof(arg);
                continue;
            }

            case 'u':
            case 'x': {
                uintptr_t arg = va_arg(copy, unsigned);
                __builtin_memcpy(buffer + arg_offset, &arg, sizeof(arg));
                arg_offset += sizeof(arg);
                continue;
            }

            case 's': {
                const char* str = va_arg(copy, const char*);
                uintptr_t arg = reinterpret_cast<uintptr_t>(str);
                if ( str != nullptr && !is_persistent(str) ) {
                    arg = reinterpret_cast<uintptr_t>(buffer + string_offset);
                    size_t size = __builtin_strlen(str) + 1;
                    __builtin_memcpy(buffer + string_offset, str, size);
                    string_offset += size;
                }
                __builtin_memcpy(buffer + arg_offset, &arg, sizeof(arg));
                arg_offset += sizeof(arg);
                continue;
            }

            default: {  // '%p'
                uintptr_t arg = reinterpret_cast<uintptr_t>(va_arg(copy, const void*));
                __builtin_memcpy(buffer + arg_offset, &arg, sizeof(arg));
                arg_offset += sizeof(arg);
                continue;
            }
        }
    }
    va_end(copy);
}

struct writer_state_t {
    char* const buffer;
    const size_t capacity;
    size_t cursor;
    size_t rendered;  // Size of the would-be-rendered text without buffer limit.
    size_t skip;      // Size of the rendered text segment that was copied already.
};

void _buffer_output(void* context, const char* data, size_t size)
{
    auto& writer = *static_cast<writer_state_t*>(context);
    size_t begin = writer.rendered;
    writer.rendered += size;
    if ( begin + size <= writer.skip || writer.cursor >= writer.capacity )
        return;

    size_t offset = writer.skip > begin ? writer.skip - begin : 0;
    size -= offset;
    data += offset;
    size = std::min(size, writer.capacity - writer.cursor);
    __builtin_memcpy(writer.buffer + writer.cursor, data, size);
    writer.cursor += size;
};

}  // anonymous namespace



bool log_buffer::is_stale(offset_t offset, uint32_t timestamp)
{
    if ( empty() )
        return true;

    // Check whether offset remains in the occupied circular interval. This does not rely
    // on timestamp order, since timestamps restart at zero after reset.
    // Note: Although very unlikely, if a stale offset matches the original record's
    // timestamp - due to many append()s between read()s - this check will incorrectly
    // treat it as valid and return false. This collision is more likely after a reset,
    // when timestamps restart from zero.
    bool retained = m_begin < m_end
                  ? offset >= m_begin && offset < m_end
                  : offset >= m_begin || offset < m_end;
    return !retained || record_t(offset).timestamp() != timestamp;
}

va_list record_t::arguments() const
{
    const void* arguments = m_pheader + 1;
    union {
        void* pointer;
        va_list list;
    } value = { const_cast<void*>(arguments) };
    return value.list;
}

std::pair<size_t, size_t>
record_t::measure_sizes(const char* format, va_list args)
{
    size_t header_args = sizeof(header_t);
    size_t strings = is_persistent(format) ? 0 : __builtin_strlen(format) + 1;
    va_list copy;
    va_copy(copy, args);
    while ( *format ) {
        if ( *format++ != '%' )
            continue;
        switch ( *format++ ) {
            case '%':
                continue;
            case 'd':
                (void)va_arg(copy, int);
                break;
            case 'u':
            case 'x':
                (void)va_arg(copy, unsigned);
                break;
            case 's': {
                const char* string = va_arg(copy, const char*);
                if ( string != nullptr && !is_persistent(string) )
                    strings += __builtin_strlen(string) + 1;
                break;
            }
            default:
                (void)va_arg(copy, const void*);
                break;
        }
        header_args += sizeof(uintptr_t);
    }
    va_end(copy);
    return { header_args, strings };
}

offset_t log_buffer::next(offset_t offset)
{
    offset += record_t(offset).size();
    assert( offset <= sizeof(m_buffer) );
    if ( offset == m_end )
        return offset;
    if ( offset == sizeof(m_buffer) || is_padding(offset) )
        offset = 0;
    return offset;
}

offset_t log_buffer::append(unsigned level, unsigned pid, uint32_t timestamp,
                            const char* format, va_list args)
{
    if ( !supported_format(format) )
        return m_end;

    riot::irq_lock lock;

    // Canonicalize an empty ring before using m_end. A reset can leave m_end different
    // from m_begin while relocating the empty ring or pre-publishing its first record.
    if ( empty() )
        m_end = m_begin;

    format = cache_format(format);

    offset_t record_offset = m_end;
    auto [header_args, strings] =
        record_t::measure_sizes(format, args);
    size_t record_size = align_up(header_args + strings, RECORD_ALIGNMENT);

    // If the new record cannot fit at m_end without overflowing m_buffer[], recalculate
    // the record size at offset 0.
    size_t needs_padding =
        record_size > sizeof(m_buffer) - m_end ? sizeof(m_buffer) - m_end : 0;
    if ( needs_padding ) {
        record_offset = 0;
        std::tie(header_args, strings) =
            record_t::measure_sizes(format, args);
        record_size = align_up(header_args + strings, RECORD_ALIGNMENT);
    }

    // Reject the new record if it exceeds the total buffer capacity.
    if ( record_size >= sizeof(m_buffer) )
        return m_end;

    // Reset-safety: m_begin advances before evicted bytes are reused, and padding is
    // committed before publishing a wrapped record. After writing the complete record,
    // a non-empty ring commits size before m_end; an empty ring pre-publishes m_end and
    // commits size last. Compiler memory barriers enforce these orders. An interrupted
    // append() may discard old records, but will never expose a partial record.

    // Reserve enough space for the new record, evicting old records as necessary.
    size_t required = record_size + needs_padding;
    size_t available = empty() ? sizeof(m_buffer)
        : (m_begin + sizeof(m_buffer) - m_end) % sizeof(m_buffer);
    while ( available < required ) {
        size_t evicted = record_t(m_begin).size();
        offset_t begin = next(m_begin);
        if ( begin == m_end ) {
            size(begin) = 0u;
            __asm__ volatile("" ::: "memory");
            m_begin = begin;

            // All records were evicted. Discard the pending tail padding and use the
            // complete buffer from offset 0 instead.
            if ( needs_padding ) {
                if ( begin != 0 ) {
                    size(0) = 0u;
                    __asm__ volatile("" ::: "memory");
                    m_begin = 0;
                    m_end = 0;
                }
                needs_padding = 0;
            }
            break;
        }

        available += (begin == 0) ? sizeof(m_buffer) - m_begin : evicted;
        m_begin = begin;
    }

    if ( needs_padding )
        size(m_end) = 0u;
    __asm__ volatile("" ::: "memory");

    offset_t string_offset = record_offset + header_args;
    record_t::header_t header = {
        static_cast<uint16_t>(record_size),
        static_cast<uint8_t>(level),
        static_cast<uint8_t>(pid),
        timestamp,
        reinterpret_cast<uintptr_t>(format),
    };

    // Stash the format string in the record’s trailing string storage if it isn't
    // persistent.
    if ( !is_persistent(format) ) {
        header.format = reinterpret_cast<uintptr_t>(&m_buffer[string_offset]);
        size_t size = __builtin_strlen(format) + 1;
        __builtin_memcpy(&m_buffer[string_offset], format, size);
        string_offset += size;
    }

    // Pack the va_list arguments into the record, stashing any non-persistent strings.
    copy_arguments(m_buffer, record_offset, string_offset, format, args);

    offset_t end = static_cast<offset_t>(record_offset + record_size);
    if ( end == sizeof(m_buffer) )
        end = 0;
    assert( end < sizeof(m_buffer) );

    // Write every header field except size.
    __builtin_memcpy(&m_buffer[record_offset + sizeof(header.size)],
                     reinterpret_cast<const uint8_t*>(&header) + sizeof(header.size),
                     sizeof(header) - sizeof(header.size));
    __asm__ volatile("" ::: "memory");

    // For an empty ring, publish the new end before making the first record visible.
    // A reset before the size commit still leaves the ring visibly empty.
    if ( m_begin == m_end ) {
        m_end = end;
        __asm__ volatile("" ::: "memory");
    }

    // The nonzero size commits the complete record data.
    size(record_offset) = header.size;
    __asm__ volatile("" ::: "memory");

    // For a non-empty ring this exposes the new record last. For an empty ring it
    // harmlessly repeats the end value published before the size commit.
    m_end = end;
    return record_offset;
}

size_t log_buffer::read(reader_state_t& reader, char* buffer, size_t size)
{
    riot::irq_lock lock;

    // Early exit for zero-byte requests or completed readers.
    if ( size == 0 || reader.record_offset == READER_END )
        return 0;

    writer_state_t writer = { buffer, size, 0, 0, 0 };

    // Start a new reader at the oldest retained record.
    if ( reader.record_offset == READER_BEGIN ) {
        if ( empty() ) {
            reader.record_offset = READER_END;
            return 0;
        }
        reader.record_offset = begin();
        reader.timestamp = record_t(reader.record_offset).timestamp();
        reader.text_offset = 0;
    }

    // If append() overwrote the current record between reads, emit a marker and resume
    // at the new oldest retained record.
    else if ( is_stale(reader.record_offset, reader.timestamp) ) {
        constexpr size_t MARKER_SIZE = sizeof(INVALIDATED_MARKER) - 1;
        assert( writer.capacity >= MARKER_SIZE );
        _buffer_output(&writer, INVALIDATED_MARKER, MARKER_SIZE);

        if ( empty() ) {
            reader.record_offset = READER_END;
            reader.text_offset = 0;
            return writer.cursor;
        }
        reader.record_offset = begin();
        reader.timestamp = record_t(reader.record_offset).timestamp();
        reader.text_offset = 0;
    }

    while ( writer.cursor < writer.capacity ) {
        record_t record(reader.record_offset);
        size_t cursor = writer.cursor;
        writer.rendered = 0;
        writer.skip = reader.text_offset;

        // Render the current record and fill the buffer up to the size.
        output_header(_buffer_output, &writer,
                      record.level(), record.pid(), record.timestamp());
        va_list args = record.arguments();
        output_format(_buffer_output, &writer,
                      reinterpret_cast<const char*>(record.format()), args);
        _buffer_output(&writer, "\n", 1);
        size_t rendered = writer.cursor - cursor;

        // Full record fit in output buffer; advance to the next record.
        if ( reader.text_offset + rendered >= writer.rendered ) {
            reader.text_offset = 0;
            offset_t next_offset = next(reader.record_offset);

            // If it was the newest retained record, mark the reader complete.
            if ( next_offset == end() ) {
                reader.record_offset = READER_END;
                break;
            }
            reader.record_offset = next_offset;
            reader.timestamp = record_t(next_offset).timestamp();
        }

        // Output buffer filled mid-record; store the rendered-text offset to resume on
        // the next call.
        else {
            reader.text_offset += rendered;
            break;
        }
    }

    // Return only bytes actually copied, excluding skipped or capacity-limited output.
    return writer.cursor;
}
