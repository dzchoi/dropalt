* DFU suffix
  dfu-util: Invalid DFU suffix signature
  dfu-util: A valid DFU suffix will be required in a future dfu-util release!!!
  https://dfu-util.sourceforge.net/dfu-suffix.1.html

  `dfu-suffix -a -v <VendorID> -p <ProductID> -d <DeviceID> -s <SerialNumber> -i <FirmwareFile>` will add a DFU suffix to the firmware file. The DFU suffix is appended at the end of the firmware binary. The DFU suffix also includes the size of the firmware.

  The DFU suffix typically contains the following information:
  - Vendor ID (VID)
  - Product ID (PID)
  - Device ID (DID)
  - Firmware length (size)
  - DFU specification version
  - DFU signature

* Allocate variables in .noinit section (NOINIT) unless initialization is necessary.

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
