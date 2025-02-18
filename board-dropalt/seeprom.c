#include "assert.h"             // for static_assert() from Riot
#include "log.h"
#include "seeprom.h"



volatile uint8_t* SmartEEPROM8 = (uint8_t*)SEEPROM_ADDR;
volatile uint16_t* SmartEEPROM16 = (uint16_t*)SEEPROM_ADDR;
volatile uint32_t* SmartEEPROM32 = (uint32_t*)SEEPROM_ADDR;

static inline void NVMCTRL_CMD(uint16_t cmd)
{
    while ( !NVMCTRL->STATUS.bit.READY ) {}
    NVMCTRL->INTFLAG.reg |= NVMCTRL_INTFLAG_DONE;
    NVMCTRL->CTRLB.reg = cmd | NVMCTRL_CTRLB_CMDEX_KEY;
    while ( !NVMCTRL->INTFLAG.bit.DONE ) {}
}

static inline void SYNC_WAIT(void)
{
    // Todo: thread_yield() while waiting? Search for "while.*\}" across the board.
    while ( NVMCTRL->SEESTAT.bit.BUSY ) {}
}

static_assert( sizeof(uint32_t) == 4u );
static_assert( sizeof(uint16_t) == 2u );
static_assert( sizeof(uint8_t) == 1u );

void* seeprom_addr(intptr_t offset)
{
    SYNC_WAIT();
    return (void*)&SmartEEPROM8[offset];
}

static inline uint8_t _EEPROM_READ8(intptr_t offset)
{
    SYNC_WAIT();
    return SmartEEPROM8[offset];
}

static inline uint16_t _EEPROM_READ16(intptr_t offset)
{
    SYNC_WAIT();
    return SmartEEPROM16[offset / 2u];
}

static inline uint32_t _EEPROM_READ32(intptr_t offset)
{
    SYNC_WAIT();
    return SmartEEPROM32[offset / 4u];
}

static inline void _EEPROM_WRITE8(intptr_t offset, uint8_t value)
{
    SYNC_WAIT();
    SmartEEPROM8[offset] = value;
}

static inline void _EEPROM_WRITE16(intptr_t offset, uint16_t value)
{
    SYNC_WAIT();
    SmartEEPROM16[offset / 2u] = value;
}

static inline void _EEPROM_WRITE32(intptr_t offset, uint32_t value)
{
    SYNC_WAIT();
    SmartEEPROM32[offset / 4u] = value;
}

#define EEPROM_READ8(offset, buf, size) \
    do { \
        *(uint8_t*)buf = _EEPROM_READ8(offset); \
        offset++; buf++; size--; \
    } while ( 0 )

#define EEPROM_READ16(offset, buf, size) \
    do { \
        *(uint16_t*)buf = _EEPROM_READ16(offset); \
        offset += 2u; buf += 2u; size -= 2u; \
    } while ( 0 )

#define EEPROM_READ32(offset, buf, size) \
    do { \
        *(uint32_t*)buf = _EEPROM_READ32(offset); \
        offset += 4u; buf += 4u; size -= 4u; \
    } while ( 0 )

#define EEPROM_WRITE8(offset, buf, size) \
    do { \
        _EEPROM_WRITE8(offset, *(uint8_t*)buf); \
        offset++; buf++; size--; \
    } while ( 0 )

#define EEPROM_WRITE16(offset, buf, size) \
    do { \
        _EEPROM_WRITE16(offset, *(uint16_t*)buf); \
        offset += 2u; buf += 2u; size -= 2u; \
    } while ( 0 )

#define EEPROM_WRITE32(offset, buf, size) \
    do { \
        _EEPROM_WRITE32(offset, *(uint32_t*)buf); \
        offset += 4u; buf += 4u; size -= 4u; \
    } while ( 0 )

#define EEPROM_UPDATE8(offset, buf, size) \
    do { \
        if ( _EEPROM_READ8(offset) != *(uint8_t*)buf ) \
            _EEPROM_WRITE8(offset, *(uint8_t*)buf); \
        offset++; buf++; size--; \
    } while ( 0 )

#define EEPROM_UPDATE16(offset, buf, size) \
    do { \
        if ( _EEPROM_READ16(offset) != *(uint16_t*)buf ) \
            _EEPROM_WRITE16(offset, *(uint16_t*)buf); \
        offset += 2u; buf += 2u; size -= 2u; \
    } while ( 0 )

#define EEPROM_UPDATE32(offset, buf, size) \
    do { \
        if ( _EEPROM_READ32(offset) != *(uint32_t*)buf ) \
            _EEPROM_WRITE32(offset, *(uint32_t*)buf); \
        offset += 4u; buf += 4u; size -= 4u; \
    } while ( 0 )



void seeprom_init(void)
{
    if ( NVMCTRL->SEESTAT.bit.RLOCK )
        NVMCTRL_CMD(NVMCTRL_CTRLB_CMD_USEER);  // Unlock E2P data write access

    LOG_DEBUG("seeprom: PARAM=0x%lx\n", NVMCTRL->PARAM.reg);
    LOG_DEBUG("seeprom: SEECFG=0x%x\n", NVMCTRL->SEECFG.reg);
    LOG_DEBUG("seeprom: SEESTAT=0x%lx\n", NVMCTRL->SEESTAT.reg);
}

size_t seeprom_size(void)
{
    static const uint8_t max_psz[] = { 0, 3, 4, 5, 5, 6, 6, 6, 6, 7, 7 };
    uint8_t psz = NVMCTRL->SEESTAT.bit.PSZ;
    uint8_t sblk = NVMCTRL->SEESTAT.bit.SBLK;

    if ( sblk == 0u ) return 0;
    if ( psz > max_psz[sblk] )
        psz = max_psz[sblk];
    return 512u << psz;
}

void seeprom_read(intptr_t offset, void* buf, size_t size)
{
    if ( size > 0u && (offset % 2u) )
        EEPROM_READ8(offset, buf, size);

    if ( size >= 2u && (offset % 4u) )
        EEPROM_READ16(offset, buf, size);

    while ( size >= 4u )
        EEPROM_READ32(offset, buf, size);

    if ( size >= 2u )
        EEPROM_READ16(offset, buf, size);

    if ( size > 0u )
        EEPROM_READ8(offset, buf, size);
}

void seeprom_write(intptr_t offset, const void* buf, size_t size)
{
    if ( size > 0u && (offset % 2u) )
        EEPROM_WRITE8(offset, buf, size);

    if ( size >= 2u && (offset % 4u) )
        EEPROM_WRITE16(offset, buf, size);

    while ( size >= 4u )
        EEPROM_WRITE32(offset, buf, size);

    if ( size >= 2u )
        EEPROM_WRITE16(offset, buf, size);

    if ( size > 0u )
        EEPROM_WRITE8(offset, buf, size);
}

void seeprom_update(intptr_t offset, const void* buf, size_t size)
{
    if ( size > 0u && (offset % 2u) )
        EEPROM_UPDATE8(offset, buf, size);

    if ( size >= 2u && (offset % 4u) )
        EEPROM_UPDATE16(offset, buf, size);

    while ( size >= 4u )
        EEPROM_UPDATE32(offset, buf, size);

    if ( size >= 2u )
        EEPROM_UPDATE16(offset, buf, size);

    if ( size > 0u )
        EEPROM_UPDATE8(offset, buf, size);
}

void seeprom_flush(void)
{
    if ( NVMCTRL->SEESTAT.bit.LOAD ) {
        LOG_DEBUG("seeprom: NVMCTRL_CTRLB_CMD_SEEFLUSH\n");
        NVMCTRL_CMD(NVMCTRL_CTRLB_CMD_SEEFLUSH);
    }
}
