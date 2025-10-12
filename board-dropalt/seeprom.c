#include "assert.h"
#include "log.h"
#include "seeprom.h"



static void NVMCTRL_CMD(uint16_t cmd)
{
    // Todo: thread_yield() while waiting? Search for "while.*\}" across the board.
    while ( !NVMCTRL->STATUS.bit.READY ) {}
    NVMCTRL->INTFLAG.reg |= NVMCTRL_INTFLAG_DONE;
    NVMCTRL->CTRLB.reg = cmd | NVMCTRL_CTRLB_CMDEX_KEY;
    while ( !NVMCTRL->INTFLAG.bit.DONE ) {}
}

void seeprom_init(void)
{
    // Check if SEEPROM space is allocated as configured.
    assert( NVMCTRL->SEESTAT.bit.SBLK == SEEPROM_SBLK
        && NVMCTRL->SEESTAT.bit.PSZ == SEEPROM_PSZ);

    if ( NVMCTRL->SEESTAT.bit.RLOCK )
        NVMCTRL_CMD(NVMCTRL_CTRLB_CMD_USEER);  // Unlock E2P data write access.

    LOG_DEBUG("seeprom: PARAM=0x%lx SEECFG=0x%x SEESTAT=0x%lx",
        NVMCTRL->PARAM.reg, NVMCTRL->SEECFG.reg, NVMCTRL->SEESTAT.reg);
}

void seeprom_flush(void)
{
    if ( NVMCTRL->SEESTAT.bit.LOAD ) {
        LOG_DEBUG("seeprom: NVMCTRL_CTRLB_CMD_SEEFLUSH");
        NVMCTRL_CMD(NVMCTRL_CTRLB_CMD_SEEFLUSH);
    }
}
