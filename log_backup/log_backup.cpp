// Implements the public logging API: it stores records in log_buffer and emits them to
// stdout. It uses a small streaming formatter instead of vsnprintf() because retained
// records must be rendered into bounded DFU buffers and resumed partway through a record
// without allocating a full rendered-record buffer.

#include <cstdarg>
#include <cstdint>

#include "compiler_hints.h"
#include "log.h"
#include "log_module.h"
#include "log_reader.h"
#include "stdio_base.h"         // for stdio_write()
#include "thread.h"
#include "ztimer.h"

#include "irq.hpp"              // for riot::irq_lock
#include "log_buffer.hpp"



namespace {

constexpr const char* COLOR_CODE[] = {
    "\e[0m",     // LOG_LUA_ERROR
    "\e[0;31m",  // LOG_ERROR (red)
    "\e[0;33m",  // LOG_WARNING (yellow)
    "\e[0;36m",  // LOG_INFO (cyan)
    "\e[0m"      // LOG_DEBUG
};

uint8_t log_mask = 0xff;

static_assert(KERNEL_PID_ISR <= UINT8_MAX);

void _stdio_output(void*, const char* data, size_t size)
{
    stdio_write(data, size);
}

void output_string(output_f output, void* context, const char* string)
{
    if ( string == nullptr )
        string = "(null)";
    output(context, string, __builtin_strlen(string));
}

void output_decimal(output_f output, void* context, uint32_t value)
{
    char digits[10];
    char* end = digits + sizeof(digits);
    char* begin = end;
    do {
        *--begin = '0' + value % 10;
        value /= 10;
    } while ( value );

    output(context, begin, end - begin);
}

void output_signed(output_f output, void* context, uint32_t value)
{
    if ( static_cast<int32_t>(value) < 0 ) {
        output(context, "-", 1);
        value = 0u - value;
    }
    output_decimal(output, context, value);
}

void output_hex(output_f output, void* context, uint32_t value)
{
    static constexpr char DIGITS[] = "0123456789abcdef";
    char digits[8];
    char* end = digits + sizeof(digits);
    char* begin = end;
    do {
        *--begin = DIGITS[value & 0xf];
        value >>= 4;
    } while ( value );

    output(context, begin, end - begin);
}

template <size_t N1, size_t N2>
void output_separator(output_f output, void* context, bool cond,
                      const char (&sep1)[N1], const char (&sep2)[N2])
{
    if ( cond )
        output(context, sep1, N1 - 1);
    else
        output(context, sep2, N2 - 1);
}

} // anonymous namespace



void output_header(output_f output, void* context,
                   unsigned level, unsigned pid, uint32_t timestamp)
{
    output_decimal(output, context, timestamp);
    output_separator(output, context, timestamp < 10000000U, "\t", " ");

    const char* name;
    if ( pid == KERNEL_PID_UNDEF )
        name = "boot";
    else if ( pid == KERNEL_PID_ISR )
        name = "isr";
    else
        name = thread_getname(pid);

    if ( name != nullptr ) {
        size_t length = __builtin_strlen(name);
        output(context, name, length);
        output_separator(output, context, length < 8, "\t\t", "\t");
    }
    else {
        output(context, "thread", 6);
        output_decimal(output, context, pid);
        output(context, "\t", 1);
    }

    // Log levels are deliberately omitted from the displayed header. To restore them,
    // use the compact single-character presentation below.
    // static constexpr char LEVEL[] = "EEWID";
    // char level_char = level < sizeof(LEVEL) - 1 ? LEVEL[level] : '?';
    // output(context, " ", 1);
    // output(context, &level_char, 1);
    // output(context, " ", 1);
    (void)level;
}

// Renders the supported log format using an ordinary ARM va_list.
// Note that it stops and prints "???" on the first unsupported format specifier.
void output_format(output_f output, void* context, const char* format, va_list args)
{
    va_list copy;
    va_copy(copy, args);
    const char* literal = format;
    while ( *format ) {
        if ( *format++ != '%' )
            continue;
        output(context, literal, format - literal - 1);

        char specifier = *format++;
        if ( !supported_specifier(specifier) ) {
            va_end(copy);
            output(context, "???", 3);
            return;
        }

        if ( specifier == '%' ) {
            output(context, "%", 1);
            literal = format;
            continue;
        }

        switch ( specifier ) {
            case 'd':
                output_signed(output, context,
                              static_cast<uint32_t>(va_arg(copy, int)));
                break;
            case 'u':
                output_decimal(output, context, va_arg(copy, unsigned));
                break;
            case 'x':
                output_hex(output, context, va_arg(copy, unsigned));
                break;
            case 's':
                output_string(output, context, va_arg(copy, const char*));
                break;
            default:  // '%p'
                output(context, "0x", 2);
                output_hex(output, context, reinterpret_cast<uintptr_t>(
                    va_arg(copy, const void*)));
                break;
        }
        literal = format;
    }

    output(context, literal, format - literal);
    va_end(copy);
}



extern "C" void log_backup(unsigned level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vlog_backup(level, format, args);
    va_end(args);
}

extern "C" void vlog_backup(unsigned level, const char* format, va_list args)
{
    // Keep serialization and CDC writes together. Besides protecting the ring state,
    // this prevents another thread from interleaving text in the CDC ACM transmit ring.
    unsigned state = irq_disable();

    // Default to 0 before ztimer_init() to avoid a null pointer dereference
    // in ztimer_now().
    uint32_t timestamp = (ZTIMER_MSEC->ops != nullptr) ? ztimer_now(ZTIMER_MSEC) : 0;

    unsigned pid = irq_is_in() ? KERNEL_PID_ISR : thread_getpid();
    offset_t offset = log_buffer::append(level, pid, timestamp, format, args);
    bool stored = (offset != log_buffer::end());

    thread_t* active = thread_get_active();
    if ( level == LOG_LUA_ERROR
      || unlikely(active == nullptr)
      || (log_mask & (1u << thread_get_priority(active))) ) {
        bool colored = *format
                    && (level == LOG_ERROR || level == LOG_WARNING || level == LOG_INFO);
        if ( colored )
            stdio_write(COLOR_CODE[level], __builtin_strlen(COLOR_CODE[level]));

        if ( stored ) {
            record_t record(offset);
            output_header(_stdio_output, nullptr,
                          record.level(), record.pid(), record.timestamp());
            output_format(_stdio_output, nullptr,
                reinterpret_cast<const char*>(record.format()), record.arguments());
        }
        else {
            output_header(_stdio_output, nullptr, level, pid, timestamp);
            output_format(_stdio_output, nullptr, format, args);
        }

        if ( colored )
            stdio_write(COLOR_CODE[LOG_DEBUG],
                        __builtin_strlen(COLOR_CODE[LOG_DEBUG]));
        stdio_write("\n", 1);
    }

    irq_restore(state);
}

extern "C" uint8_t get_log_mask(void)
{
    return log_mask;
}

extern "C" void set_log_mask(uint8_t mask)
{
    unsigned state = irq_disable();
    log_mask = mask;
    irq_restore(state);
}



extern "C" void log_reader_prep(reader_state_t* reader)
{
    riot::irq_lock lock;
    reader->record_offset = READER_BEGIN;
}

extern "C" size_t log_reader_read(reader_state_t* reader, char* buffer, size_t size)
{
    return log_buffer::read(*reader, buffer, size);
}
