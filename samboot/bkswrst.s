@ bkswrst.s  Bank Swap and Reset (BKSWRST) for Drop Alt mdloader flashing.
@
@ When mdloader flashes the combined image, the SAM-BA bootloader (0x00000x3FFF)
@ runs first and jumps to this app at 0x4000.  The vector table is placed here;
@ the reset handler immediately issues BKSWRST, which swaps Bank A/B and resets.
@ After the reset Bank B (holding the monolithic firmware) maps to 0x0000 and the
@ monolithic firmware begins executing.
@
@ Register constants are taken directly from:
@   riot/cpu/sam0_common/include/vendor/samd51/include/samd51j18a.h
@   riot/cpu/sam0_common/include/vendor/samd51/include/component/nvmctrl.h
@   riot/cpu/sam0_common/include/vendor/samd51/include/component/pac.h

        .syntax unified
        .cpu cortex-m4
        .thumb

@ -- Constants ---------------------------------------------------------------

        @ NVMCTRL register layout
        .equ NVMCTRL_BASE,        0x41004000
        .equ NVMCTRL_CTRLB_OFF,   0x04        @ 16-bit: write commands here
        .equ NVMCTRL_INTFLAG_OFF, 0x10        @ 16-bit: DONE flag in bit 0
        .equ NVMCTRL_STATUS_OFF,  0x12        @ 16-bit: READY flag in bit 0

        @ CTRLB value: CMD = 0x17 (BKSWRST) in bits[6:0],
        @              CMDEX = 0xA5 (KEY) in bits[15:8]
        .equ NVMCTRL_CTRLB_VAL,   0xA517

        @ Top of 128 KB SRAM
        .equ STACK_TOP,           0x20020000

@ -- Vector table (placed at the very start = device address 0x4000) ---------

        .section .isr_vector, "a", %progbits
        .align  2
_vectors:
        .word   STACK_TOP               @ Initial stack pointer (MSP)
        .word   reset_handler           @ Reset vector (assembler sets Thumb bit)

@ -- Reset handler -----------------------------------------------------------

        .section .text
        .thumb_func
        .global reset_handler
        .type   reset_handler, %function
reset_handler:
        cpsid   i                       @ Disable all interrupts

        ldr     r2, =NVMCTRL_BASE

        @ Wait until NVM controller is idle.
        @   while (!NVMCTRL->STATUS.bit.READY) {}
.Lwait_ready:
        ldrh    r3, [r2, #NVMCTRL_STATUS_OFF]
        tst     r3, #1                  @ STATUS.READY
        beq     .Lwait_ready

        @ Clear the DONE interrupt flag before issuing the command.
        @   NVMCTRL->INTFLAG.reg |= NVMCTRL_INTFLAG_DONE;
        ldrh    r3, [r2, #NVMCTRL_INTFLAG_OFF]
        orr     r3, r3, #1
        strh    r3, [r2, #NVMCTRL_INTFLAG_OFF]

        @ Issue BKSWRST.  The device resets here; everything below is unreachable.
        @   NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMD_BKSWRST | NVMCTRL_CTRLB_CMDEX_KEY;
        ldr     r3, =NVMCTRL_CTRLB_VAL
        strh    r3, [r2, #NVMCTRL_CTRLB_OFF]

        @ Safeguard spin (hardware reset fires before this completes).
.Lwait_done:
        ldrh    r3, [r2, #NVMCTRL_INTFLAG_OFF]
        tst     r3, #1
        beq     .Lwait_done
.Lhang:
        b       .Lhang

        .ltorg                          @ Emit literal pool here
        .size   reset_handler, . - reset_handler
