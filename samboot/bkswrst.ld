/*
 * bkswrst.ld  Linker script for the BKSWRST application (Drop Alt).
 *
 * The app is loaded by mdloader starting at device address 0x4000 (right after
 * the 16 KB SAM-BA bootloader).  FW_ROM_LEN caps the binary to 112 KB so it
 * cannot overflow into Bank B (0x20000) where the monolithic firmware resides.
 */

ENTRY(reset_handler)

MEMORY
{
    rom (rx)  : ORIGIN = 0x00004000, LENGTH = 0x1C000  /* 112 KB */
    ram (rwx) : ORIGIN = 0x20000000, LENGTH = 0x20000  /* 128 KB SRAM */
}

SECTIONS
{
    .isr_vector :
    {
        *(.isr_vector)
    } > rom

    .text :
    {
        *(.text*)
        *(.rodata*)
    } > rom
}
