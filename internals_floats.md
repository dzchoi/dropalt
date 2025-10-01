* Cortex-M4F provides hardware support for 32-bit floating-point operations, but lacks native support for 64-bit double-precision values.

* Printf-style formatting is more straightforward than std::cout, especially when using %x compared to std::hex().

* However, '%f' is better disabled to reduce footprint (~6KB). It is disabled by default in newlib-nano, but importing Lua enables '%f' via the linker flag `-u _printf_float`.

* Lua package in RIOT explicitly includes `_printf_float` to enable floating-point formatting support.
  - This module adds ~6,200 bytes to the final binary size.
  ```
  # riot/pkg/lua/Makefile.dep
  USEMODULE += printf_float
  ```

  ```
  # riot/sys/Makefile.include
  ifneq (,$(filter printf_float,$(USEMODULE)))
    ifneq (,$(filter newlib_nano,$(USEMODULE)))
      LINKFLAGS += -u _printf_float
    endif
  endif
  ```

* `_printf_float` is required solely for printing floating-point numbers. Floating-point computations work correctly without it.
  - Without it, `print(3.)` would show `.0`.
  - Needed for `print()` and `string.format()`.
  ```
  # luaconf.h
  #define lua_number2str(s,sz,n)  \
	l_sprintf((s), sz, LUA_NUMBER_FMT, (LUAI_UACNUMBER)(n))

  #define LUA_NUMBER_FMT  "%.7g"
  #define LUAI_UACNUMBER  double

  #define lua_str2number(s,p)	strtof((s), (p))
  ```

* AAPCS
  - Integer args: Passed in r0–r3
  - Floating-point args: Passed in s0–s15 (if VFP is enabled like Cortex-M4F)
  - Floats in variadic functions: Promoted to double
    So, `printf()` operates only on doubles, not directly on floats.
  - Stack alignment: Must be 8-byte aligned
  - Return values: In r0 (or s0 for float/double)
