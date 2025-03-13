#### Configuration

* Using `USEPKG += lua` will automatically download Lua 5.3.4 source files into riot/build/pkg/lua, and patch them.

* Lua uses tlsf-malloc() for allocating a dynamic memory on the pre-allocated `uint8_t lua_memory[LUA_MEM_SIZE]`.
  ```
  USEPKG += tlsf
  USEMODULE += lua-contrib
  USEMODULE += printf_float
  ```

* luaconf.h
  ```
  #define LUA_32BITS  // Enable Lua with 32-bit integers and 32-bit floats.
  #define LUA_INT_TYPE	LUA_INT_INT
  #define LUA_FLOAT_TYPE	LUA_FLOAT_FLOAT
  #define LUA_NUMBER	float

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

#### Reducing binary size

* Following the suggestion from [Lua Technical Note 2](https://www.lua.org/notes/ltn002.html) for optimizing the binary size.

* Excluding parsing modules:  
  The Lua core is adjusted to exclude the parsing modules (lcode.c, llex.c, lparser.c). Since we do not compile Lua source code at run-time on the keyboard, these modules are not necessary. The keyboard will either store a pre-compiled Lua chunk in flash memory or receive one from the host PC, which has been compiled using `luac` or `daluac`.

  Refer to lua_emb/Makefile.dep to see how `USEMODULE += lua-contrib_noparser` excludes these modules.

  This limits `lua_load()` to accept only pre-compiled chunks and load them onto the stack using lundump. It will cause an error if a string chunk is given.

  `lua_dump()` and `string.dump()` are disabled accordingly.

* Disabling `dofile()` and `loadfile()`:  
  The functions dofile() and loadfile() are disabled in the base module. Although they can automatically detect pre-compiled chunks, they are not useful in applications without a file system. Instead, use load(), which can specifically load pre-compiled Lua chunks.

  The following functions are removed accordingly:
  - luaL_dofile(), luaL_loadfile() and luaL_loadfilex()
  - luaL_dostring() and luaL_loadstring()

* Changing `require()` Behavior:
  > A C library statically linked to Lua can register its luaopen_ function into the preload table (`package.preload`), so that it will be called only when (and if) the user requires that module.

  The `require()` function is modified to support only one searcher, the preload searcher, which is selected directly without using the package.searchers table. The package.preload table now includes two loaders: one for loading the Lua bytecode in flash memory (`require "keymap"`), and another for loading the "fw" module (`require "fw"`).

  Note: The package library (lua_loadlib.c, which replaces loadlib.c) provides only the require() function and includes only the package.loaded and package.preload tables. It does not contain other tables such as package.searchers or package.cpath.

* Difference between C modules and Lua modules
  - C modules (modules written in C) are loaded using `luaL_requiref()`. They have to provide an `luaopen_...` function, which will be called by luaL_requiref().

    The `luaopen_...(modname)` can:
    - Place all or some methods in the global environment `_G` and return _G.
    - Create a table with all or some methods as entries and return the table.

    Additionally, we can choose whether to place the table in `_G` when calling luaL_requiref(), just like e.g. `math = require "math"`.

  - Lua modules (modules written in Lua) are loaded using `require()`.
    The entire module is executed to return the module table.

* lua_run.c contains several C functions related to executing the Lua interpreter. However, as we set up our own interpreter execution environment, those functions are not used, including `lua_riot_do_buffer()` and `lua_riot_do_module()`. The only function used from lua_run.c is `lua_riot_newstate()`, which is for setting up tlsf_malloc().

* The interpreter name will be "dalua" and the compiler, "daluac".

#### Reduce memory footprint

* Lua stack size  
  The default stack limit is one million entries.

* Support for "frozen tables" (i.e. tables that live in ROM).
  - https://stackoverflow.com/questions/23236262/lua-opcodes-in-flash-memory
  - https://eluaproject.net/doc/v0.9/en_arch_ltr.html
  - https://github.com/higaski/Lua-5.1.5-TR

  > Each time you register a module (via luaL_register) you create a new table and populate it with the module's methods. But a table is a read/write datatype, so luaL_register is quite inefficient if you don't plan to do any write operations on that table later (adding new elements or manipulating existing ones).

#### Porting the RIOT patches for updating Lua 5.3.4 -> 5.3.6

Note: riot/pkg/pkg.mk uses `git am` to apply the patches.

0. Clean up the Lua source directory  
   $ rm -fr riot/build/pkg/lua

1. Update riot/pkg/lua/Makefile with the new baseline commit. Check the tag "v5.3.6" and commit ID by running `git log` on the latest Lua repo (https://github.com/lua/lua.git).
   ```
   # tag: v5.3.6
   PKG_VERSION=75ea9ccbea7c4886f30da147fb67b693b2624c26
   ```

2. Rename the directory:
   $ mv riot/pkg/lua/patches riot/pkg/lua/patches.old.

3. Try to build the firmware. It will likely fail, but riot/build/pkg/lua will have downloaded the prestine Lua source files with the new tag.

4. For each .patch file in riot/pkg/lua/patches.old, apply the patch manually in the order of the patch file number.

   $ patch -p1 <.../riot/pkg/lua/patches.old/0001-Remove-luaL_newstate.patch

   Ensure that each patch is applied successfully. If it fails, edit the source files manually. Once done, commit the changes using the commit message in the patch file.

5. Create the new patch files in riot/pkg/lua/patches/.

   $ git format-patch -<number_of_commits> -o riot/pkg/lua/patches/

6. Check if the new patch files work.
   $ rm -fr riot/build/pkg/lua
   $ rm -r .build
   $ make riotboot/flash-slot0
