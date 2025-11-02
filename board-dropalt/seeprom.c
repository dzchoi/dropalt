#include "assert.h"
#include "board.h"              // for sam0_flashpage_aux_cfg(), ...
#include "log.h"
#include "seeprom.h"



static void NVMCTRL_CMD(uint16_t cmd)
{
    while ( !NVMCTRL->STATUS.bit.READY ) {}
    NVMCTRL->INTFLAG.reg |= NVMCTRL_INTFLAG_DONE;
    NVMCTRL->CTRLB.reg = cmd | NVMCTRL_CTRLB_CMDEX_KEY;
    while ( !NVMCTRL->INTFLAG.bit.DONE ) {}
}

void seeprom_init(void)
{
#ifdef RIOTBOOT
    // Set up SEEPROM if not set up yet. Changes will take effect after reset.
    if ( NVMCTRL->SEESTAT.bit.SBLK != SEEPROM_SBLK
      || NVMCTRL->SEESTAT.bit.PSZ != SEEPROM_PSZ ) {
        nvm_user_page_t user_page = *sam0_flashpage_aux_cfg();
        user_page.smart_eeprom_blocks = SEEPROM_SBLK;
        user_page.smart_eeprom_page_size = SEEPROM_PSZ;
        char* s = (char*)sam0_flashpage_aux_get(0);
        s = (*s == '\xff' ? NULL : __builtin_strdup(s));

        // Update SEEPROM_SBLK and SEEPROM_PSZ in the USER page while preserving the
        // reserved section (the first 32 bytes).
        sam0_flashpage_aux_reset(&user_page);

        // Restore the product serial.
        if ( s ) {
            sam0_flashpage_aux_write(0, s, __builtin_strlen(s) + 1);
            __builtin_free(s);
        }

        enter_bootloader();
        UNREACHABLE();
    }
#else
    // Check if SEEPROM space is allocated as configured.
    assert( NVMCTRL->SEESTAT.bit.SBLK == SEEPROM_SBLK
        && NVMCTRL->SEESTAT.bit.PSZ == SEEPROM_PSZ );

    if ( NVMCTRL->SEESTAT.bit.RLOCK )
        NVMCTRL_CMD(NVMCTRL_CTRLB_CMD_USEER);  // Unlock E2P data write access.

    LOG_DEBUG("seeprom: PARAM=0x%lx SEECFG=0x%x SEESTAT=0x%lx",
        NVMCTRL->PARAM.reg, NVMCTRL->SEECFG.reg, NVMCTRL->SEESTAT.reg);
#endif
}

void seeprom_flush(void)
{
    if ( NVMCTRL->SEESTAT.bit.LOAD ) {
        LOG_DEBUG("seeprom: NVMCTRL_CTRLB_CMD_SEEFLUSH");
        NVMCTRL_CMD(NVMCTRL_CTRLB_CMD_SEEFLUSH);
    }
}
