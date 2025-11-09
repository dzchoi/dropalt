# Monolithic Firmware

## Overview

- The bootloader and firmware are compiled and linked into a single image; no separate bootloader is used.
- Supports both DFU mode and normal operation.
- The firmware image is headerless and omits the first 1 KB slot header typically included in applications targeting Riotboot.
- The image is flashed to Bank B of the SAMD51’s dual-bank flash memory. A Bank Swap Reset (BKSWRST) is then triggered to exchange Bank A and Bank B, activating and rebooting from the newly flashed image.
- After flashing a new firmware image, the Lua Keymap module is erased and must be downloaded again.
- The image size is limited to 112 KB (128 KB minus 16 KB), as each of Bank A and Bank B occupies 128 KB of flash, with 16 KB reserved for SEEPROM. A key advantage of this layout is that SEEPROM contents persist through BKSWRST.

## `dfu-util` slots

- Only one slot is exposed to `dfu-util`: **slot 0**.
  ```
  $ dfu-util -l
  dfu-util 0.9

  Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
  Copyright 2010-2016 Tormod Volden and Stefan Schmidt
  This program is Free Software and has ABSOLUTELY NO WARRANTY
  Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

  Found DFU: [04d8:eed3] ver=0093, devnum=17, cfg=1, intf=2, path="3-4.2", alt=0, name="RIOT-OS Slot 0", serial="09HMMKAG040334"
  ```

  - If a **headerless image** is flashed, it is treated as new firmware. A BKSWRST is performed to reboot into the new image.
  - If a **headered image** is flashed, it is treated as Lua bytecode. The system continues in normal mode without rebooting.
  - Uploading from slot 0 is supported and retrieves logs stored in backup RAM, regardless of the image type present in Bank B.

## DFU Mode vs Normal Mode

- The `usb_thread` runs CDC + DFU (not DFU Runtime) + HID stacks concurrently, allowing `DFU_DNLOAD` and `DFU_UPLOAD` requests to be handled immediately without rebooting.
- Unlike `DFU_UPLOAD`, `DFU_DNLOAD` requires stopping the current Lua Keymap execution and Lua REPL, since the new image is flashed into Bank B, where the Keymap module resides. As a result, both the `matrix_thread` and Lua runtime are stopped until flashing completes and BKSWRST reboots the device into the new image.
- The `usb_thread` keeps the DFU USB stack active at all times. During a `DFU_DNLOAD` operation, this state is referred to as dedicated DFU mode (or simply **DFU mode**), while the regular operating state is referred to as **normal mode**.
  - Example: calling `fw.dfu_mode()` transitions the device into dedicated DFU mode.
- At startup, the system determines whether to enter DFU mode (e.g., due to a hard fault) or normal mode (e.g., after a power-on or system reset).

## Manifestation Tolerance

- The DFU USB stack advertises being manifestation tolerant, so it does not automatically reboot after downloading.
- Use `dfu-util -R` to explicitly trigger a reboot if needed.

## DFU_UPLOAD

- `DFU_UPLOAD` is handled to capture backup RAM logs in both DFU mode and normal mode.

## DFU_DNLOAD Workflow

- Upon receiving the first `DFU_DNLOAD` request (during the Setup stage):
  - The `usb_thread` signals the `matrix_thread` to disable key event handling.
  - It instructs the `main_thread` to enter DFU mode (`FLAG_MODE_TOGGLE`).
  - The `usb_thread` transitions to the `DFU_DL_BUSY` state and immediately flashes the first payload (block number 0).
- Both headerless images (firmware) and headered images (Lua bytecode) are supported.
- The first payload is written directly to Bank B. This is safe because:
  - Firmware images do not affect the currently running code on Bank A and will boot after manifestation.
  - Lua bytecode payloads begin with a 1 KB slot header (mostly zeros), allowing Lua to reload and continue running.
  - Therefore, a `wTransferSize` of up to 1 KB is supported.
- After flashing the first payload:
  - The `usb_thread` waits for the `main_thread` to enter DFU mode.
  - It then transitions to the `DFU_DL_IDLE` state.
- The `main_thread`:
  - Turns off LEDs.
  - Destroys the Lua runtime environment.
  - Waits for the `matrix_thread` to become idle before entering DFU mode.
  - Ignores all key events until `FLAG_MODE_TOGGLE` is received.
- For subsequent `DFU_DNLOAD` requests (block number ≥ 1):
  - The `usb_thread` flashes each payload immediately.
  - Transitions to `DFU_DL_IDLE` after each transfer.
- After the final `DFU_DNLOAD` request (`wLength = 0`):
  - The `usb_thread` signals the `matrix_thread` to re-enable key event handling.
  - Behavior depends on image type:
    - **Firmware image (headerless):**
      - The `usb_thread` waits for a `DFU_DETACH` request to trigger BKSWRST.
      - The `main_thread` remains in DFU mode, awaiting a key event (e.g., ESC key) to manually trigger BKSWRST.
    - **Lua bytecode (headered):**
      - The `usb_thread` signals `FLAG_MODE_TOGGLE` to the `main_thread`, allowing it to exit DFU mode and return to normal operation.

## Logging

- The firmware can output logs in both DFU mode and normal mode.
- Logs can be captured by `dfu-util` at any time.

## Legacy Flashing via mdloader

- The monolithic firmware can also be flashed using the legacy `mdloader`.
- A large combined image is written, consisting of:
  - A small application at address `0x00000` that performs BKSWRST.
  - Padding filled with `0xFF` bytes to align the monolithic firmware at Bank B (`0x20000`).
  - The monolithic firmware itself.
