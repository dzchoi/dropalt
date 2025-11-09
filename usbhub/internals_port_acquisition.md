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
  Tracks the host port selected by usbhub_select_host_port(). Defined in the .noinit section, it persists across reboots, except after POR or NVM resets (if corrupted during flashing).
- last_host_port:
  Captures the host port successfully acquired during the initial boot sequence. It does not reflect port changes due to switchovers.

## Acquisition principle
All boot types except power-on reset and system reset are intended to enter DFU mode. In DFU mode, the system should continue using the host port (`current_host_port`) that was active prior to the reboot. In normal mode, the system defaults to `last_host_port`.
