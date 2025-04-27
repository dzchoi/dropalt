#pragma once

#include <stddef.h>             // for size_t
#include <stdint.h>             // for uint8_t
#include "cpu_conf.h"           // for NVMCTRL



#ifdef __cplusplus
extern "C" {
#endif

// Compute the SEEPROM virtual size based on SBLK and PSZ. Refer to Table 25-6 in the SAM
// D5x datasheet.
static inline size_t seeprom_size(void)
{
    static const uint8_t max_psz[] = { 0, 3, 4, 5, 5, 6, 6, 6, 6, 7, 7 };
    const uint8_t sblk = SEEPROM_SBLK;
    uint8_t psz = SEEPROM_PSZ;

    if ( sblk == 0u ) return 0;
    if ( psz > max_psz[sblk] )
        psz = max_psz[sblk];
    return 512u << psz;
}

static inline void* seeprom_addr(size_t offset) {
    return (void*)((uint8_t*)SEEPROM_ADDR + offset);
}

static inline void seeprom_sync(void) {
    while ( NVMCTRL->SEESTAT.bit.BUSY ) {}
}

static inline void seeprom_unbuffered(void) {
    NVMCTRL->SEECFG.reg = NVMCTRL_SEECFG_WMODE_UNBUFFERED;
}

static inline void seeprom_buffered(void) {
    NVMCTRL->SEECFG.reg = NVMCTRL_SEECFG_WMODE_BUFFERED;
}

void seeprom_init(void);

// Writes to SEEPROM in buffered mode are buffered when bits are changed from 1 to 0
// and no read is attempted on the written data. The seeprom_flush() flushes any buffered
// data that are not written yet.
void seeprom_flush(void);

#ifdef __cplusplus
}
#endif
