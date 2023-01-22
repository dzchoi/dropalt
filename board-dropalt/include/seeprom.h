#pragma once

#include <stdint.h>             // for uint*_t
#include <stddef.h>             // for size_t



#ifdef __cplusplus
extern "C" {
#endif

void seeprom_init(void);
size_t seeprom_size(void);
void seeprom_read(intptr_t offset, void* buf, size_t size);
void seeprom_write(intptr_t offset, const void* buf, size_t size);
void seeprom_update(intptr_t offset, const void* buf, size_t size);

// Write to Seeprom is buffered as long as the bits are changed only from 1 to 0 and
// no read is attempted on the written data. The seeprom_flush() flushes any buffered data
// that are not written yet.
void seeprom_flush(void);

#ifdef __cplusplus
}
#endif
