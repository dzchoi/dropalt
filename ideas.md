* How to check the max stack size and detect the stack overflow?

* Redefine Riot-independent #define constants using "static const" and "static inline".

* Configure the automatic switchover feature in config.hpp.

* Change m_pthread->flags directly instead calling thread_flags_set(), if we don't need to yield to other threads at this moment, and there is no other threads or interrupt that can change it simultaneously (So irq_disable() is not necessary).

* lua.hpp to include lauxlib.h

[Compile multiple Lua files]
* E.g. luac -o combined.luac file1.lua file2.lua file3.lua
* See https://www.lua.org/source/5.3/luac.c.html#combine
```
  for (i=0; i<argc; i++)
  {
    const char* filename=IS("-") ? NULL : argv[i];
    if (luaL_loadfile(L,filename)!=LUA_OK) fatal(lua_tostring(L,-1));
  }
  f=combine(L,argc);
  ...
  lua_lock(L);
  luaU_dump(L,f,writer,D,stripping);
  lua_unlock(L);
```
* `luaU_dump()` is used instead of `lua_dump()`.
  `lua_dump()` operates on a LClosure (Lua function) at the top of the stack and calls your custom writer function to output the bytecode.
  `luaU_dump()` operates directly on a `Proto` structure (the compiled representation of a Lua function), and `lua_dump()` calls `luaU_dump()` internally.
* [Chained module composition] Each module shall be compiled as an anonymous function and shall be composed with each other, the result(s) resturned from previous module will be passed to the next module as argument(s). The first module will take the module name as its only argument, and the final module will return the module table as its only result.

* USB: Is the USB access delay (m_delay_usb_accessible|_tmo_usb_accessible) still necessary on recent Linux?

* Fix: "CDCACM: line coding not supported".

* Switchover: Is DTE disconnected when switchover happens manually?

* RGB: Precise color accuracy isn't necessary — keycaps give everything a reddish hue.

* `assert( false )` and `assert( status == LUA_OK )` are not the proper way to shutdown. Use abort(), lua_error() or luaL_error() instead.
  `assert()` for only "non-trivial" logical error.

* Tips
  - `typedef struct lua_State lua_State;`
  - CFLAGS from parent Makefile are inherited, but the changes in child Makefiles do not propagate back.
  - Use likely(x) (== __builtin_expect((uintptr_t)(x), 1)) if appropriate.
  - Allocate variables in ".noinit" section (NOINIT) unless initialization is strictly necessary.
  - Favor fputs() over printf() and puts() if possible to minimize code size.
  - Don't waste assert(). Even a simple `assert( m_pthread );` consumes 56 bytes.
  - Don't include <cstdbool>, <cstddef> and <cstdint> for each source file, as they would
    have been already included in the header file that provides the functions with those
    types as parameters or as a return value.

[Size optimization]
* Binary size is also affected by .data section. Walk through those variables that initialize to non-zero values.
* Replace printf() with cout, calling usbus_cdc_acm_flush() when '\n' is hit. LOG_*() can also be implemented using an ostream.

* Use MODULE_CORE_IDLE_THREAD to enable CPU sleep when idle.

* Implement the firmware uploading feature
  Currently, we can retrieve the logs stored in the device's backup ram using the upload command of `dfu-util`. While also uploading a firmware binary is not essential, it is still a nice feature to have. The implementation would be straightforward, but we need to determine the image size beforehand. This can be pre-written when generating a firmware image with the slot header included (`slot0.XXXX.bin`). We could consider extending riotboot_hdr_t in riot/sys/include/riotboot/hdr.h..."

[Bootloader Updater]
* Headerless application
* Provides slot 0 in dfu-util to flash the real bootloader (at the first 16KB).
* Allocate and initialize SEEPROM, or hardfault will occur otherwise.
* Maybe we could utilize an intermediary bootloader to flash the final bootloader, using memory banks and switching them.

* Bootloader as a shared library
  The bootloader, rather than the firmware, could function as a shared library (.so) because it undergoes fewer changes. This shared library could potentially allocate a reset vector at address 0. Additionally, symbols exported from the bootloader might be able to resolve those in the firmware.

* Store a format string and its parameters for a log
  All parameters will be 4 bytes in size, except for doubles.

* Where will be the UART pins?
