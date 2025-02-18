#pragma once

#include <stddef.h>             // for size_t
#include "cpu_conf.h"           // for NVMCTRL



#ifdef __cplusplus
extern "C" {
#endif

void seeprom_init(void);
size_t seeprom_size(void);
void seeprom_read(intptr_t offset, void* buf, size_t size);
void seeprom_write(intptr_t offset, const void* buf, size_t size);
void seeprom_update(intptr_t offset, const void* buf, size_t size);
void* seeprom_addr(intptr_t offset);

static inline void seeprom_unbuffered(void) {
    NVMCTRL->SEECFG.reg = NVMCTRL_SEECFG_WMODE_UNBUFFERED; }

static inline void seeprom_buffered(void) {
    NVMCTRL->SEECFG.reg = NVMCTRL_SEECFG_WMODE_BUFFERED; }

// Writing to Seeprom in Buffered mode gets buffered when bits are changed from 1 to 0
// and when no read is attempted on the written data. The seeprom_flush() flushes any
// buffered data that are not written yet.
void seeprom_flush(void);

#ifdef __cplusplus
}
#endif
