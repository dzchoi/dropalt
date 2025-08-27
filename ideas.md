* Fix: "CDCACM: line coding not supported".

[Refactor]
* Rename reboot_to_bootloader -> jump_to_bootloader in board-dropalt, Lua and bootloader.
* Rename .value -> .uint?. Search for "union" across sources.
* Rename key_queue -> main_key_events, key_event_queue_t -> usb_key_events.
* `assert( false )` and `assert( status == LUA_OK )` are not the proper way to shutdown. Use abort(), lua_error() or luaL_error() instead.
  `assert()` for only "non-trivial" logical error.
* Manage all thread stack sizes in one place.

[Keymap]
* Context-remembering keymaps:
  Pressing End the first time triggers End, then triggers Home on the 2nd and 3rd
  presses. Can be implemented using TapSeq with a longer tapping_term_ms.

[Debouncing]
* DEBOUNCE_PRESS_MS and DEBOUNCE_RELEASE_MS defined in NVM.

[Switchover]
* Configure the automatic switchover feature in config.hpp.
* Switchover event should be given to Effects.

[RGB]
* Precise color accuracy isn't necessary — keycaps show everything in a reddish color.
* RGB is enabled or disabled dynamically from Effects in Lua?
* Adjustable threshold values (ADC_CON1_NOMINAL) in NVM?
* Calls to c_active_effect is enabled only if ENABLE_RGB_LED = true.

[REPL]
* REPL chunks execute only when matrix_thread and usb_thread are idle.
* `Function(f)` executes only when matrix_thread and usb_thread are idle.
  --> Put `f` in an array and execute it later.
* Switchover as lamp event.
* matrix_thread.hpp:
  ```
  static bool matrix_thread::is_any_pressed() { return m_wakeup_us != 0; }
  ```
* usb_thread.hpp:
  ```
  #include "thread.h"             // for thread_get_status(), STATUS_FLAG_BLOCKED_ANY

  // Return true if no events are being handled or waiting in the queue.
  // Note that the thread stays in STATUS_MUTEX_BLOCKED during ztimer_periodic_wakeup().
  static bool usb_thread::is_idle() {
      return thread_get_status(m_pthread) == STATUS_FLAG_BLOCKED_ANY;
  }
  ```

[Tips]
* Redefine Riot-independent #define constants using "static const" and "static inline".
* Change m_pthread->flags directly instead calling thread_flags_set(), if we don't need to yield to other threads at this moment, and there is no other threads or interrupt that can change it simultaneously (So irq_disable() is not necessary).
* `uint_fast8_t` instead of `unsigned` if the full size is not necessary. E.g. slot_index.
* `typedef struct lua_State lua_State;`
* CFLAGS from parent Makefile are inherited, but the changes in child Makefiles do not propagate back.
* Use `likely(x)` (== `__builtin_expect((uintptr_t)(x), 1)`) if appropriate.
* Allocate variables in ".noinit" section (NOINIT) unless initialization is strictly necessary.
* Favor fputs() over printf() and puts() if possible to minimize code size.
* Don't waste assert(). Even a simple `assert( m_pthread );` consumes 56 bytes.
* Don't include <cstdbool>, <cstddef> and <cstdint> for each source file, as they would
  have been already included in the header file that provides the functions with those
  types as parameters or as a return value.
* Binary size is also affected by .data section. Walk through those variables that initialize with non-zero values.

[Log]
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

* Use MODULE_CORE_IDLE_THREAD to enable CPU sleep when idle.

[Bootloader Updater]
* Headerless application
* E.g. `make riotboot/slot0` generates two binaries: one with a header and one without.
* Generic updater that does not setup EEPROM. Can flash ANY bootloader of size <= 16KB.
* `dfu-util` can flash a bootloader at slot 0.
* Two types are generated: slot0.bin and slot0.xxx.bin
* Provides slot 0 in dfu-util to flash the real bootloader.
* Allocate and initialize SEEPROM, or hardfault will occur otherwise.
* Maybe we could utilize an intermediary bootloader to flash the final bootloader, using memory banks and switching them.
* Debug led pattern during execution?
* Upload the existing bootloader:
  Assuming it is the first(?) application placed immediately after the bootloader, it uploads the bootloader image of any size, excluding the trailing 0xFFs.

[Bootloader]
* Do not reboot after downloading (flashing); `dfu-util` provides `-R` as a separate option.
* Upload the userpage.
* Implement the firmware uploading feature
  Currently, we can retrieve the logs stored in the device's backup ram using the upload command of `dfu-util`. While also uploading a firmware binary is not essential, it is still a nice feature to have. The implementation would be straightforward, but we need to determine the image size beforehand. This can be pre-written when generating a firmware image with the slot header included (`slot0.XXXX.bin`). We could consider extending riotboot_hdr_t in riot/sys/include/riotboot/hdr.h..."
* Bootloader as a shared library
  The bootloader, rather than the firmware, could function as a shared library (.so) because it undergoes fewer changes. This shared library could potentially allocate a reset vector at address 0. Additionally, symbols exported from the bootloader might be able to resolve those in the firmware.

[Keymap recovery]
* Default keymaps are stored in a byte array in the firmware so that it is selected when a "custom" keymaps at flash slot 1 fails to load (or run).

* Where will be the UART pins?
