* Fix: "CDCACM: line coding not supported".

[Version]
* Should indicate the current version.

[Debouncing]
* DEBOUNCE_PRESS_MS and DEBOUNCE_RELEASE_MS defined in NVM.

[Lua]
* Handle NORETURN functions
  ```
  $ dalua -e 'fw.system_reset()'
  dalua: no response from serial port
  ```
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
* C++ 11 has the `[[noreturn]]` attribute.
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
