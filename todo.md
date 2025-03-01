* Allocate variables in .noinit section (NOINIT) unless initialization is strictly necessary.

* Check max stack size.

* Use likely(x) (== __builtin_expect((uintptr_t)(x), 1)) if appropriate.

* Redefine Riot-independent #define constants using "static const" and "static inline".

* Use MODULE_CORE_IDLE_THREAD to enable CPU sleep when idle.

* SEEPROM
  ```
  board-dropalt/Makefile.include:
  # Each slot length should be a multiple of 8K (block size for EB command).
  NUM_SLOTS       = 2
  SLOT0_LEN       = 0x0001C000  # 112K = 256K/2 - 16K (bootloader)
  SLOT1_LEN       = 0x0001C000  # 112K = 256K/2 - SLOT_AUX_LEN

  # SEEPROM is allocated at the end of NVM.
  SLOT_AUX_LEN    = 0x00004000  # 16K = 2 * SBLK * 8K

  PROGRAMMERS_SUPPORTED += dfu-util

  include $(RIOTMAKE)/boards/sam0.inc.mk
  ```

* Implement the firmware uploading feature
  Currently, we can retrieve the logs stored in the device's NVM using the upload command of `dfu-util`. While uploading a firmware binary is not essential, it is still a nice feature to have. The implementation would be straightforward, but we need to determine the image size beforehand. This can be pre-written when generating a firmware image with the slot header included (`slot0.XXXX.bin`). We could consider extending riotboot_hdr_t in riot/sys/include/riotboot/hdr.h..."

* Will it be possible to flash a bootloader without debugger?
  Maybe we could utilize an intermediary bootloader to flash the final bootloader, using memory banks and switching them.
