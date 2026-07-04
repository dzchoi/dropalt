## Reset types
- 0x01 (POR): Power-on reset
- 0x08 (NVM): BKSWRST after flashing with DFU_DNLOAD
- 0x10 (EXT): Triggered by pressing the Debug pin or programming via EDBG
- 0x20 (WDT): Watchdog timer reset
- 0x40 (SYS): System reset
- 0x80 (BKUP): Backup reset
- Both `core_panic()` and `fw.dfu_mode()` trigger a system reset with the magic number "RIOT".

## `current_host_port` vs `last_host_port`
- current_host_port:
  Tracks the host port selected by usbhub_select_host_port(). Stored in `.backup.noinit` (backup RAM) as a member of `struct backup_t`, it persists across reboots without being re-initialized.

  `.backup.noinit` is required (rather than the ordinary `.noinit` section in main RAM) because `current_host_port` must remain at the same address even after flashing a new firmware. Variables in `.noinit` are placed after `.bss` in the linker layout, so their addresses shift whenever `.data` or `.bss` grows or shrinks  for example when new modules, thread stacks, or global variables are added. After a BKSWRST (NVM reset), the new firmware would then read from the wrong address and see a garbage value. Backup RAM (`bkup_ram`) is a separate 8 KB region at a fixed physical address (0x47000000), independent of the main RAM layout. By placing `current_host_port` as the first non-log member of `struct backup_t`, its offset within backup RAM is pinned regardless of how main RAM is arranged in any firmware version.

- last_host_port:
  Captures the host port successfully acquired during the initial boot sequence. It does not reflect port changes due to switchovers.

## Acquisition principle
All boot types except power-on reset and system reset are intended to enter DFU mode. In DFU mode, the system should continue using the host port (`current_host_port`) that was active prior to the reboot. In normal mode, the system defaults to `last_host_port`.
