* Fix: "CDCACM: line coding not supported".

[Debouncing]
* DEBOUNCE_PRESS_MS and DEBOUNCE_RELEASE_MS defined in NVM.

[Lua]
* Configure the automatic switchover feature in config.hpp.
* Switchover event should be given to Effects.
* Switchover as lamp event.
* Expose `request_extra_enable_manually/automatically()` in `fw`.
* Upload the userpage (512 bytes) using `uuencode`.

[RGB]
* Precise color accuracy isn't necessary — keycaps show everything in a reddish color.
* RGB is enabled or disabled dynamically from Effects in Lua?
* Adjustable threshold values (ADC_CON1_NOMINAL) in NVM?

[dalua]
* Do not assume the remote status ("[}0\n") appears on its own line. The preceding line may not end with a newline character.
* Support `-d`, `-p` and `-S` options like dfu-util does.

[Tips]
* Redefine Riot-independent #define constants using "static const" and "static inline".
* Change m_pthread->flags directly instead calling thread_flags_set(), if we don't need to yield to other threads at this moment, and there is no other threads or interrupt that can change it simultaneously (So irq_disable() is not necessary).
* `typedef struct lua_State lua_State;`
* CFLAGS from parent Makefile are inherited, but the changes in child Makefiles do not propagate back.
* Use `likely(x)` (== `__builtin_expect((uintptr_t)(x), 1)`) if appropriate.
* Allocate variables in ".noinit" section (NOINIT) unless initialization is strictly necessary.
* Favor stdio_write() over printf() and fputs() if possible to minimize code size.
* Don't waste assert(). Even a simple `assert( m_pthread );` consumes 56 bytes. Use `assert()` for only "non-trivial" logical error.
* Don't include <cstdbool>, <cstddef> and <cstdint> for each source file, as they would
  have been already included in the header file that provides the functions with those
  types as parameters or as a return value.
* Binary size is also affected by .data section. Walk through those variables that initialize with non-zero values.

[Power consumption]
* Use MODULE_CORE_IDLE_THREAD to enable CPU sleep when idle.

[Formatted logs]
* Log messages are segmented and stored in backup RAM, then reconstructed using a host-side utility.
* Each log record includes:
  record size, thread ID, log level, timestamp, format string address, and corresponding arguments (each 4 bytes except for 8-byte doubles).
* Consider the Default Argument Promotions for variadic functions in C/C++.
* Prior to transmitting log entries, metadata containing all format strings and their respective addresses is sent first.

[No bootloading]
* A single application that can also work as bootloader. It supports both DFU mode and normal mode operations. The application is headerless.
* The application can occupy the full 128KB, which corresponds to one bank of the dual memory.
* The application can output logs, which can be captured by dfu-util anytime during DFU mode, as long as Lua script is flashed.

* Startup
  - Starts with the DFU (only) mode first, then enters the normal mode immediately if:
    = no magic number (for DFU mode),
    = no error reset, and
    = Lua script is ready on other memory bank.
  - Stay in DFU (only) mode until a key is pressed, then enter the normal mode.
  - Firmware download is started in DFU mode. Disable the keyboard interrupt. Then,
    = Performs the bank switch and reboot, if firmware download completes.
    = Jump to the normal mode, if Lua script download completes.
  - No Lua running and no RGBs durng DFU mode.

* Normal mode
  - The normal mode provides DFU, CDC and HID stacks simultaneously.
  - Jump (not reboot) to DFU mode if DFU_DOWNLOAD is received.

* The application is flashed into the alternate bank of dual memory (e.g. A -> B -> A -> ...).
* When flashed using the legacy `mdloader`, a large combined image is written, which consists of:
  - Small machine code that swaps the flash banks and triggers a system reset
    (Set up SEEPROM?)
  - 112KB of padding (0xFFs) to offset the application at other bank
  - The application.
* On boot, the application determines whether to start with DFU mode or normal mode.
* After flashing the application, the Lua chunk code and seeprom (NVM) must also be reflashed.
* Only one slot is provided to flash [slot 0].
  Uploading from slot 0 will upload the logs in the backup RAM if the image at slot 0 (i.e. the alternate bank of dual memory) is NOT a compiled Lua chunk. Otherwise, it will upload the entire 128KB image at slot 0, excluding trailing 0xFFs.
  The entire image can be the existing application or the old DropALT bootloader plus firmware.
* backup_ram_init() is called only at the entry of normal mode. Thus, logs are collected during DFU mode, but will be erased when entering normal mode.
* DEL key exists DFU mode.

* riot/sys/include/usb/dfu.h
```
#define USB_DFU_WILL_DETACH             0x08    /**< DFU Detach capability attribute */

#define USB_DFU_PROTOCOL_RUNTIME_MODE   0x01 /**< Runtime mode */
#define USB_DFU_PROTOCOL_DFU_MODE       0x02 /**< DFU mode */

    USB_DFU_STATE_APP_IDLE,               /**< DFU application idle */
    USB_DFU_STATE_APP_DETACH,             /**< DFU application detach (reboot to DFU mode) */
    USB_DFU_STATE_DFU_IDLE,               /**< DFU runtime mode idle */
    USB_DFU_STATE_DFU_DL_SYNC,            /**< DFU download synchronization */
```
* See riot/tests/sys/usbus_board_reset/main.c for usbus + cdc_acm + dfu.

[Bootloader Updater]
* Headerless application
* pkg->index indicates the interface number (i.e. slot number).
* Upload the existing bootloader. [slot 0]
  Assuming it is the first(?) application placed immediately after the bootloader, it uploads the bootloader image of any size, excluding the trailing 0xFFs.
* Two types of images are generated: slot0.bin and slot0.xxx.bin
* E.g. `make riotboot/slot0` generates two binaries: one with a header and one without.
* Generic updater that does not setup EEPROM. Can flash ANY bootloader of size <= 16KB.
* `dfu-util` can flash a bootloader at slot 0.
* Provides slot 0 in dfu-util to flash the real bootloader.
* Allocate and initialize SEEPROM, or hardfault will occur otherwise.
* Maybe we could utilize an intermediary bootloader to flash the final bootloader, using memory banks and switching them.
* Debug led pattern during execution?

[Bootloader]
* Flash and start both headered and headerless images.
  `dfu->skip_signature = true` only if the firmware image includes "RIOT" at the slot header.
* Save the debug print for boot reason (e.g. watchdog fault) using `backup_ram_write(format, args)` when entering bootloader.
* Upload the log messages. [slot 1]
* Implement the firmware uploading feature
  Currently, we can retrieve the logs stored in the device's backup ram using the upload command of `dfu-util`. While also uploading a firmware binary is not essential, it is still a nice feature to have. The implementation would be straightforward, but we need to determine the image size beforehand. This can be pre-written when generating a firmware image with the slot header included (`slot0.XXXX.bin`). We could consider extending riotboot_hdr_t in riot/sys/include/riotboot/hdr.h..."
* Bootloader as a shared library
  The bootloader, rather than the firmware, could function as a shared library (.so) because it undergoes fewer changes. This shared library could potentially allocate a reset vector at address 0. Additionally, symbols exported from the bootloader might be able to resolve those in the firmware.

[Keymap recovery]
* Default keymaps are stored in a byte array in the firmware so that it is selected when a "custom" keymaps at flash slot 1 fails to load (or run).

* Where will be the UART pins?



* Similar to thread_flags_clear(), but clears one flag at a time.
```
static thread_flags_t check_thread_flag_one(thread_t* pthread, thread_flags_t mask)
{
    unsigned state = irq_disable();
    thread_flags_t lsb = pthread->flags & mask;
    lsb &= -lsb;
    pthread->flags &= ~lsb;
    irq_restore(state);
    return lsb;
}
```
