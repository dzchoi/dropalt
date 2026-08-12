#pragma once

#include <stddef.h>
#include <stdint.h>



#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t offset_t;

// Context for reading log records continuously.
typedef struct reader_state_t {
    // READER_BEGIN starts at log_buffer::begin(); READER_END returns no more records.
    // Otherwise, this is the record currently being processed.
    offset_t record_offset;

    // Timestamp signature of the record at record_offset. Ignored if record_offset is a
    // sentinel.
    uint32_t timestamp;

    // Byte offset into the current record's rendered text (for resuming partial reads).
    uint32_t text_offset;
} reader_state_t;

// Initializes reader state to stream retained records as newline-delimited text.
void log_reader_prep(reader_state_t* reader);

// Renders log records into output buffer. Returns the number of bytes written.
size_t log_reader_read(reader_state_t* reader, char* buffer, size_t size);

#ifdef __cplusplus
}
#endif
