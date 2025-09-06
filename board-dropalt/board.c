#include "backup_ram.h"         // for backup_ram_init()
#include "board.h"
#include "mpu.h"                // for mpu_configure(), mpu_enable(), ...
#include "periph/gpio.h"
#include "periph/wdt.h"
#include "sr_exp.h"             // for sr_exp_init()



/*
#include "vectors_cortexm.h"

// Override ISR vectors provided from vectors_cortexm.c with those from QMK's startup.c,
// including reset_handler_default(). However, cpu/samd5x/vectors.c already provides
// weak function definitions such as isr_dmac0 and isr_tc4.
extern void EIC_0_Handler(void);
extern void EIC_1_Handler(void);
...
extern void EIC_15_Handler(void);
extern void DMAC_0_Handler(void);
extern void SERCOM1_0_Handler(void);
extern void USB_0_Handler(void);
extern void USB_1_Handler(void);
extern void USB_2_Handler(void);
extern void USB_3_Handler(void);
extern void TC4_Handler(void);

ISR_VECTOR(1) const isr_t exception_table[] = {
    [ 12] = EIC_0_Handler,
    [ 13] = EIC_1_Handler,
    ...
    [ 27] = EIC_15_Handler,
    [ 31] = DMAC_0_Handler,
    [ 50] = SERCOM1_0_Handler,
    [ 80] = USB_0_Handler,
    [ 81] = USB_1_Handler,
    [ 82] = USB_2_Handler,
    [ 83] = USB_3_Handler,
    [111] = TC4_Handler
};
*/

/*
#include "time_units.h"         // for US_PER_SEC

static uint32_t usec_delay_mult;
#define USEC_DELAY_LOOP_CYCLES 3  // machine cycles per loop in clk_busy_wait_us

void clk_busy_wait_us(uint32_t usec)
{
    (void)usec;
    __asm__ volatile (
        "CBZ R0, return\n\t"    // If usec == 0, branch to return label
        "MULS R0, %0\n\t"       // Multiply R0 (usec) by usec_delay_mult
        ".balign 16\n\t"        // Ensure loop is aligned for fastest performance
        "loop: SUBS R0, #1\n\t" // Subtract 1 from R0 and update flags (1 cycle)
        "BNE loop\n\t"          // Branch if non-zero to loop label (2 cycles)
                                // NOTE: USEC_DELAY_LOOP_CYCLES is cycles per loop.
        "return:\n\t"           // Return label
        :                       // No output registers
        : "r" (usec_delay_mult) // For %0
    );
    // Note: BX LR generated
}

    // set up clk_busy_wait_us() (in board_init()).
    usec_delay_mult =
        sam0_gclk_freq(SAM0_GCLK_MAIN) / (USEC_DELAY_LOOP_CYCLES *US_PER_SEC);
    if ( usec_delay_mult == 0u )
        usec_delay_mult++;
*/


static void _dfll_usbcrm_init(void)
{
    // configure and enable DFLL for USB clock recovery mode at 48MHz
    OSCCTRL->DFLLCTRLA.bit.ENABLE = 0;
    while ( OSCCTRL->DFLLSYNC.bit.ENABLE || OSCCTRL->DFLLSYNC.bit.DFLLCTRLB ) {}

    /* workaround for Errata 2.8.3 DFLLVAL.FINE Value When DFLL48M Re-enabled */
    OSCCTRL->DFLLMUL.reg = OSCCTRL_DFLLMUL_MUL(48000)  // 48000KHz
    // "When using DFLL48M in USB recovery mode, the Fine Step value must be 0xA to
    // guarantee a USB clock at +/-0.25% before 11 ms after a resume."
    // The USB gets enabled by writing a '1' to CTRLA.ENABLE.
                         | OSCCTRL_DFLLMUL_FSTEP(0xA);
    OSCCTRL->DFLLCTRLB.reg = 0;                  /* Select Open loop configuration */
    OSCCTRL->DFLLCTRLA.bit.ENABLE = 1;           /* Enable DFLL */
    OSCCTRL->DFLLVAL.reg = OSCCTRL->DFLLVAL.reg; /* Reload DFLLVAL register */
    OSCCTRL->DFLLCTRLB.reg =
        OSCCTRL_DFLLCTRLB_MODE | OSCCTRL_DFLLCTRLB_USBCRM | OSCCTRL_DFLLCTRLB_CCDIS;

    while ( OSCCTRL->DFLLSYNC.bit.ENABLE || !OSCCTRL->STATUS.bit.DFLLRDY ) {}
}


// RIOTBOOT is defined only when building the bootloader.
#ifndef RIOTBOOT
// Override the weak post_startup() function in riot/cpu/cortexm_common/vectors_cortexm.c.
// This function is called before cpu_init(), board_init() and kernel_init().
void post_startup(void)
{
    backup_ram_init();
}
#endif

// Initialization flow:
// reset_handler_default()
//     --> cpu_init() --> periph_init()
//     --> board_init()
//     --> kernel_init() --> auto_init() --> ztimer_init()
//                       --> main()
// Beware: ztimer_sleep() and ztimer_now() are not avaiable until ztimer_init() is
//   called. Use clk_busy_wait_us() above instead.
//
// The cpu_init() that is called before board_init() initializes GCLK0_MAIN and
// GCLK1_32KHZ, and GCLK_SOURCE_DFLL in open-loop mode also calls periph_init().

void board_init(void)
{
    // Initialize the Debug LED.
    gpio_init(LED0_PIN, GPIO_OUT);
    LED0_OFF;

#ifdef BTN0_PIN
    // Initialize the on-board button.
    gpio_init(BTN0_PIN, BTN0_MODE);
#endif

#ifndef RIOTBOOT
    // Nullptr access protection
    // Once the firmware boots, we relocate the vector table and protect the first 32
    // bytes at 0x00000000 to catch null and near-null pointer dereferences (e.g.
    // 0->foo), which will raise a Mem Manage fault if accessed.
    mpu_disable();
    mpu_configure(
        0,  // MPU region #0 (MPU_NOEXEC_RAM also uses region #0 but is not active)
        0,  // RAM base address 0x00000000
        MPU_ATTR(1, AP_NO_NO, 0, 1, 0, 1, MPU_SIZE_32B)  // No exec/read/write
    );
    mpu_enable();

    // Bootloader does not use WDT.
    wdt_setup_reboot(0u, WDT_TIMEOUT_MS);
    wdt_start();
#endif

    // Initialize Shift Register.
    sr_exp_init();

    // Set GCLK_SOURCE_DFLL in USB recovery mode.
    // Todo: Check the DFLL status register to see if the DFLL is locked to the SOF_1KHZ
    //   signal. The DFLL status register typically contains flags indicating the lock
    //   status and the source of the reference clock.
    _dfll_usbcrm_init();
}
