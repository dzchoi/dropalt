* Fix: "CDCACM: line coding not supported".

[Refactor]
* Rename .value -> .uint?. Search for "union" across sources.
* Rename key_queue -> main_key_events, key_event_queue_t -> usb_key_events.
* `assert( false )` and `assert( status == LUA_OK )` are not the proper way to shutdown. Use abort(), lua_error() or luaL_error() instead.
  `assert()` for only "non-trivial" logical error.
* Manage all thread stack sizes in one place.

[Debouncing]
* DEBOUNCE_PRESS_MS and DEBOUNCE_RELEASE_MS defined in NVM.

[Switchover]
* Configure the automatic switchover feature in config.hpp.
* Switchover event should be given to Effects.
* Switchover as lamp event.

[RGB]
* Precise color accuracy isn't necessary — keycaps show everything in a reddish color.
* RGB is enabled or disabled dynamically from Effects in Lua?
* Adjustable threshold values (ADC_CON1_NOMINAL) in NVM?

[Lua]
* Upload the userpage (512 bytes).

[Tips]
* Redefine Riot-independent #define constants using "static const" and "static inline".
* Change m_pthread->flags directly instead calling thread_flags_set(), if we don't need to yield to other threads at this moment, and there is no other threads or interrupt that can change it simultaneously (So irq_disable() is not necessary).
* `uint_fast8_t` instead of `unsigned` if the full size is not necessary. E.g. slot_index.
* `typedef struct lua_State lua_State;`
* CFLAGS from parent Makefile are inherited, but the changes in child Makefiles do not propagate back.
* Use `likely(x)` (== `__builtin_expect((uintptr_t)(x), 1)`) if appropriate.
* Allocate variables in ".noinit" section (NOINIT) unless initialization is strictly necessary.
* Favor stdio_write() over printf() and fputs() if possible to minimize code size.
* Don't waste assert(). Even a simple `assert( m_pthread );` consumes 56 bytes.
* Don't include <cstdbool>, <cstddef> and <cstdint> for each source file, as they would
  have been already included in the header file that provides the functions with those
  types as parameters or as a return value.
* Binary size is also affected by .data section. Walk through those variables that initialize with non-zero values.

[Log]
* Newlib-nano does not include `%f` support in printf, sprintf, etc.
  You must explicitly enable it using the linker flag: `-u _printf_float`.
* LOG_DEBUG() and fw.log() add '\n' automatically at the end.
* Replace printf() with cout, calling usbus_cdc_acm_flush() when '\n' is hit. LOG_*() can also be implemented using an ostream.
* Store a format string and its parameters for a log record.
  All parameters will be 4 bytes in size, except for doubles.
  Consider Default Argument Promotion Rules for variadic functions in C/C++.
* C++ logger (Refer to C11 _Generic())
```
#define print(x) _Generic((x), \
    int: print_int, \
    float: print_float, \
    double: print_double, \
    char*: print_str, \
    const char*: print_str \
)(x)

void print_int(int x) { printf("%d\n", x); }
void print_float(float x) { printf("%f\n", x); }
void print_double(double x) { printf("%lf\n", x); }
void print_str(const char* x) { printf("%s\n", x); }
```
* Formatted log: Current time, thread name, color, ...
* `fw.log()` automatically appends '\n'. Otherwise, it disrupts `dalua`.
* `thread_get_active()` returns NULL from interrupt context.

* Use MODULE_CORE_IDLE_THREAD to enable CPU sleep when idle.

[No bootloader]
* The system uses a single firmware application without a separate bootloader. This application supports both DFU mode and normal operation.
* The application is headerless.
* The application is flashed into the alternate bank of dual memory (e.g. A -> B -> A -> ...).
* When flashed using the legacy `mdloader`, a large combined image is written—including 112KB of padding followed by the application. The initial portion of this padding contains machine code that swaps the flash banks and triggers a system reset.
* On boot, the application determines whether to enter DFU mode or normal mode.
* After flashing the application, the Lua chunk code and seeprom (NVM) must also be reflashed.

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
* pkg->index(?) indicates the slot number.
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

[Bootloader itself as bootloader updater]
* Bootloader is built as a relocatable code so that it can be flashed in any region of the memory.
* It checks whether it started as a firmware or as a bootloader (at 0x0).
* If it started as an application:
  - Wait for dfu-util, then flash the bootloader.
  - Wait for a user key press, then copy its own code into the bootloader region.

[Bootloader]
* wait_for_host_connection() starts from the host port on which `enter_bootloader()` executed.
* Do not reboot after downloading (flashing); `dfu-util` provides `-e` and `-R` as a separate option. No USB_DFU_WILL_DETACH?
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
