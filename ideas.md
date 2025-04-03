* How to check the max stack size and detect the stack overflow?

* Redefine Riot-independent #define constants using "static const" and "static inline".

* Use a static class (having only static methods/members) instead of a singleton class for defining each thread.

* Configure the automatic switchover feature in config.hpp.

* Fix: "CDCACM: line coding not supported".

* Tips
  - typedef struct lua_State lua_State;
  - CFLAGS from parent Makefile are inerited, but the changes here do not propagate back.
  - Use likely(x) (== __builtin_expect((uintptr_t)(x), 1)) if appropriate.
  - Allocate variables in ".noinit" section (NOINIT) unless initialization is strictly necessary.
  - Favor fputs() over printf() and puts() if possible to minimize code size.
  - Don't waste assert(). Even a simple `assert( m_pthread );` consumes 56 bytes.
  - Don't include <cstdbool>, <cstddef> and <cstdint> for each source file, as they would
    have been already included in the header file that provides the functions with those
    types as parameters or as a return value.

* Binary size is also affected by .data section. Walk through those variables that initialize to non-zero values.

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

* Use MODULE_CORE_IDLE_THREAD to enable CPU sleep when idle.

* Implement the firmware uploading feature
  Currently, we can retrieve the logs stored in the device's backup ram using the upload command of `dfu-util`. While also uploading a firmware binary is not essential, it is still a nice feature to have. The implementation would be straightforward, but we need to determine the image size beforehand. This can be pre-written when generating a firmware image with the slot header included (`slot0.XXXX.bin`). We could consider extending riotboot_hdr_t in riot/sys/include/riotboot/hdr.h..."

* Will it be possible to flash a bootloader without debugger?
  Maybe we could utilize an intermediary bootloader to flash the final bootloader, using memory banks and switching them.

* Store a format string and its parameters for a log
  All parameters will be 4 bytes in size, except for doubles.

* Where will be the UART pins?

* Bootloader as a shared library
  The bootloader, rather than the firmware, could function as a shared library (.so) because it undergoes fewer changes. This shared library could potentially allocate a reset vector at address 0. Additionally, symbols exported from the bootloader might be able to resolve those in the firmware.
