* Using `USEPKG += lua` will automatically download Lua 5.3.4 source files into riot/build/pkg/lua, and patch them.

* Lua uses tlsf-malloc() for allocating a dynamic memory on the pre-allocated `uint8_t lua_memory[LUA_MEM_SIZE]`.
  ```
  USEPKG += tlsf
  USEMODULE += lua-contrib
  USEMODULE += printf_float
  ```

* Lua core has been adjusted to exclude the parsing modules (lcode, llex, lparser) to minimize the firmware size. Refer to lua_emb/Makefile.dep to see how `USEMODULE += lua-contrib_noparser` is defined.

* luaconf.h
  ```
  #define LUA_32BITS  // Enable Lua with 32-bit integers and 32-bit floats.
  #define LUA_INT_TYPE	LUA_INT_INT
  #define LUA_FLOAT_TYPE	LUA_FLOAT_FLOAT
  #define LUAL_BUFFERSIZE   64
  ```
  LUA_USE_LINUX is not #defined.

* LUA is not compatible with picolibc and raises errors at compile time:
  ```
  lua/liolib.c:671:38: error: '_IOFBF' undeclared (first use in this function)
  lua/liolib.c:671:46: error: '_IOLBF' undeclared (first use in this function)
  ```

  FEATURES_BLACKLIST += picolibc

  ```
  ifneq (,$(filter newlib_syscalls_default,$(USEMODULE)))
    USEMODULE += libc_gettimeofday
  endif
  ```

* Support for "frozen tables" (i.e. tables that live in ROM).
  - https://stackoverflow.com/questions/23236262/lua-opcodes-in-flash-memory
  - https://eluaproject.net/doc/v0.9/en_arch_ltr.html
  - https://github.com/higaski/Lua-5.1.5-TR

  > Each time you register a module (via luaL_register) you create a new table and populate it with the module's methods. But a table is a read/write datatype, so luaL_register is quite inefficient if you don't plan to do any write operations on that table later (adding new elements or manipulating existing ones).

  Lua 5.2 supports light C functions.

* Then, do we need `require(...)`? `lua_riot_do_module_or_buf()`?
