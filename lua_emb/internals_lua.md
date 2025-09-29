#### Configuration

* Using `USEPKG += lua` will automatically download Lua 5.3.6 source files into riot/build/pkg/lua, and patch them.

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

#### Modules

* Standard libraries

* "keymap" module
  It executes in two ways. It executes entirely during power-up, i.e. when loading the module. And...

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

  The `require()` function is customized to support only one searcher: the preload searcher, which is directly selected without utilizing the package.searchers table. The package.preload table exists but it's currently empty. Two modules ("keymap" and "fw") are already implemented in the firmware. These modules are preloaded using a simplified version of "require".

  Note: The package library (lua_loadlib.c, which replaces loadlib.c) provides only the require() function and implements only the package.loaded and package.preload tables. It does not contain other tables such as package.searchers or package.cpath.

* Difference between C modules and Lua modules
  - C modules (modules written in C) are loaded using `luaL_requiref()`. They have to provide an `luaopen_...` function, which will be called by luaL_requiref().

    The `luaopen_...(modname)` can:
    - Place all or some methods in the global environment `_G` and return _G.
    - Create a table with all or some methods as entries and return the table.

    Additionally, we can choose whether to place the table in `_G` when calling luaL_requiref(), just like e.g. `math = require "math"`.

  - Lua modules (modules written in Lua) are loaded using `require()`.
    The entire module is executed to return the module table.

  - How to put a custom loader into package.preload from C API:
    ```
    // Configure the preload table for the "keymap" module
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    lua_pushcfunction(L, _custom_loader);
    lua_setfield(L, 1, "custom_module");
    lua_pop(L, 1);
    ```

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

#### Lua REPL

We employ a simple protocol for CDC-ACM stdin and stdout. Data sent via stdout consists of plain text strings intended for display on the host, while data received via stdin is Lua bytecode, executed by the device's Lua interpreter. This approach allows bytecode binary data to be transmitted without requiring a specialized protocol, such as xmodem.

Once the bytecode is received, the Lua interpreter executes it and sends the result (or an error message) back to the host as plain text. This output is displayed on the host's serial terminal (`dalua`). Finally, the execution status is returned to the host, indicating that the interpreter is ready for the next bytecode.

#### Flow control

The Linux CDC-ACM driver is aware of flow control settings, but its behavior with flow control characters depends on how it is configured. Here’s how it works:

1. Hardware Flow Control:
  - The CDC-ACM driver supports hardware flow control, typically implemented using RTS (Request to Send) and CTS (Clear to Send) signals. However, this requires the underlying hardware (UART) and USB-to-serial device to support hardware flow control as well.

2. Software Flow Control:
  - The driver can also support software flow control using XON (0x11) and XOFF (0x13) characters. This is typically handled at the terminal level using standard termios settings.
  - For software flow control to work, the XON/XOFF handling needs to be enabled explicitly using the tcsetattr() system call or a similar configuration utility.

  - In Linux, you can enable software flow control by setting the IXON (enable XON) and IXOFF (enable XOFF) flags using tcsetattr(). This means that the terminal driver will automatically pause data transmission upon receiving the XOFF character (0x13) and resume it when it receives the XON character (0x11).
    ```
    struct termios options;
    tcgetattr(fd, &options);       // Get current terminal attributes
    options.c_iflag |= (IXON | IXOFF); // Enable XON/XOFF
    tcsetattr(fd, TCSANOW, &options);  // Apply changes immediately
    ```

3. Driver Awareness:
  - By default, the CDC-ACM driver passes all characters, including flow control characters like XON/XOFF, transparently to user space. If software flow control is disabled, these characters are treated as regular data.
  - When flow control is enabled, the terminal layer (configured via termios) interprets these characters and manages the flow control state. The CDC-ACM driver itself doesn’t process these characters but facilitates their interpretation by the higher layers.

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

#### Check the validity of the Lua bytecode received from stdin
// Check if the input from wait_for_input() is valid Lua bytecode.
bool timed_stdin::is_lua_bytecode()
{
    if ( m_read_ahead == 0 )
        return false;

    constexpr union {
        char str[5];
        uint32_t num;
    } signature = { LUA_SIGNATURE };

    bool res = (m_read_ahead >= sizeof(uint32_t))
        && (*(uint32_t*)(void*)m_read_buffer == signature.num);
    if ( !res ) {
        l_message("Lua: not bytecode \"%.*s\"", m_read_ahead, m_read_buffer);
        m_read_ahead = 0;
    }

    return res;
}

#### REPL control flow
@startuml
title REPL control flow

participant "timed_stdin::" as stdin
participant main_thread as main
participant "repl::" as repl
participant "Host DTE" as dte

stdin <[#green]- dte: ping
stdin -[#red]> main: FLAG_DTE_READY
stdin <- main: enable()
activate stdin
stdin --> main
deactivate stdin

main -> repl: start()
activate repl
repl -[#green]> dte: pong
main <-- repl
deactivate repl

====

stdin <[#green]- dte: chunk1
stdin <- main: wait_for_input()
activate stdin
stdin -[#red]> main: FLAG_EXECUTE_REPL
stdin --> main
deactivate stdin

main -> repl: execute()
activate repl

stdin <- repl: _reader()
activate stdin
stdin -[#green]> repl: chunk1
stdin --> repl
deactivate stdin

loop
stdin <- repl: _reader()
activate stdin
stdin <[#green]- dte: chunkN
stdin -[#green]> repl: chunkN
stdin --> repl
deactivate stdin
end
repl -[#green]> dte: result

main <-- repl
deactivate repl

@enduml
