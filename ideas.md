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

[Safety concern]
Omitting FLAG_GENERIC_EVENT from DFU’s wait mask means it is not consumed; the flag remains pending and will be handled on return to normal mode. That behavior is fine, and it does not block the other DFU flags.

The missing case is Lua timers:
  - Lua timer expiry posts _timer_t::m_event_timeout into m_event_queue.
  - A DFU transition destroys the Lua state.
  - lua_close() runs each started timer’s __gc, which stops its ztimer, but it does not remove an event that was already queued.
  - Returning to normal mode could then dispatch a queued event whose _timer_t belonged to the destroyed Lua state. That is unsafe, particularly because the next Lua initialization can reuse that memory.

signal_lamp_state() is different: its event is static and can safely remain queued through DFU, if preserving the most recent host lamp state is desired.

So I would not handle generic events in DFU, but I would drain the existing generic-event queue immediately after lua::global_lua_state::destroy() and before entering DFU:
```
lua::global_lua_state::destroy();

while ( event_get(&m_event_queue) ) {
    // Discard events that may refer to the destroyed Lua state.
}
```

Do not clear FLAG_GENERIC_EVENT: leaving it set is harmless—normal mode will do one empty queue pass—and avoids racing a new valid event posted concurrently. Future static lamp events posted during DFU will then still be handled after normal mode resumes.

[An infinite Lua loop in REPL]
Yes—this is a real limitation of the current single-owner Lua design.
While lua::repl::execute() is running, main_thread cannot process FLAG_KEY_EVENT, timer callbacks, or generic events. The matrix still detects transitions and pushes them into main_key_events, but a long-running REPL command can fill that finite queue. After that, key handling becomes delayed and can eventually hit the queue’s deadlock/reset protection.

The idle check prevents starting a REPL command during an in-flight key action, but it cannot bound how long the command runs after it starts. An infinite Lua loop is the worst case: the keyboard becomes unresponsive until watchdog reset—if enabled—or indefinitely.

I’d treat REPL code as untrusted and add an execution budget. The most robust approach is a Lua instruction-count hook:
  - Install lua_sethook(..., LUA_MASKCOUNT, N) before executing REPL code.
  - Have the hook abort with a controlled Lua error once a wall-clock deadline or instruction budget is exceeded.
  - Clear the hook afterward, including error paths.
  - Keep the budget short enough to preserve keyboard responsiveness, e.g. 10–50 ms.

A wall-clock deadline is preferable to instruction count alone, since a C function invoked by Lua can consume substantial time without executing Lua VM instructions. But the instruction hook handles ordinary Lua loops very well.

Yielding/retrying key processing during an arbitrary Lua call is much harder: normal Lua code is not automatically resumable unless you run it in a coroutine and require it to yield cooperatively. For a keyboard firmware, I’d keep REPL commands bounded and advise fw.execute_later()/timers for any deliberately deferred work.

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
* Rule of thumb on 32-bit ARM: for register-passed values (returns, params, locals) prefer the native register width (uint32_t / unsigned). Use small types (uint8_t, int8_t, bitfields) only when it saves memory.

[Optimizations]
* `IS31_LEDS` table (105 entries  2 bytes = 210 B). `driver` only needs 1 bit and `reg_g`
  only uses banks A, D, G, J. Two ways to shrink it:
  - Option A  split arrays (~90 B saved, keeps readability). Store `reg_g` as
    `uint8_t[105]` (105 B) + a driver bitmask (14 B) = 119 B vs 210 B. Access cost:
    `(driver_bits[i>>3] >> (i&7)) & 1`.
  - Option B  pack to 1 byte/entry (105 B saved). The data only ever uses banks A, D, G,
    J  i.e. offsets 0, 48, 96, 144. So `reg_g = bank*48 + num`, bank fits in 2 bits, num
    in 4 bits, `driver` in 1 bit ? 7 bits, one byte. Keep the readable enum source and
    pack at compile time.

* With `LTO = 1` in the Makefile, Riot passes -flto only during linking, not during C/C++ compilation. Adding `CFLAGS += $(LTOFLAGS)` enables true cross-file optimization, allowing small C++ methods to be inlined across translation units.

[Power consumption]
* Use MODULE_CORE_IDLE_THREAD to enable CPU sleep when idle.

[Keymap recovery]
* Default keymaps are stored in a byte array in the firmware so that it is selected when a "custom" keymaps at flash slot 1 fails to load (or run).

[Plan SSEEPROM disable during DFU]
Microchip documents that SBLK is loaded from the USER page only at reset. BKSWRST relocates SmartEEPROM only when SBLK != 0; with SmartEEPROM enabled, both firmware banks must reserve its flash area. Microchip’s dual-bank update documentation
The proposed sequence would be:
```
Normal firmware, SBLK=1
        │
        ├─ flush and snapshot 4 KiB SmartEEPROM
        ├─ write USER.SBLK=0
        └─ system reset
                │
DFU preparation boot, SBLK=0
        │
        ├─ do not let seeprom_init() re-enable it
        ├─ accept up to 128 KiB into the inactive bank
        └─ BKSWRST
                │
New firmware boot, SBLK=0
        │
        ├─ write USER.SBLK=1, PSZ=3
        └─ system reset
                │
New firmware, SBLK=1
        ├─ initialize/recreate SmartEEPROM
        ├─ restore snapshot
        └─ clear update transaction state
```

The important complications are:
* Disabling must happen before DFU starts writing. It cannot be done upon receiving the first DFU packet because changing SBLK requires a reset, which would terminate that transfer.
* The current [`seeprom_init()` (line 23)](/home/stem/projects/atsamd51/dropalt/board-dropalt/seeprom.c:23) immediately restores SBLK=1 whenever it sees a mismatch. It would need to recognize the “DFU prepared” state and temporarily skip that behavior.
* With SBLK=0, [`BKSWRST` at line 154 (line 154)](/home/stem/projects/atsamd51/dropalt/usb/usb_dfu.cpp:154) will not relocate the old SmartEEPROM. The incoming full-bank image will overwrite its raw flash area. Persistent settings therefore need to be snapshotted or intentionally reset to defaults.
* The 8 KiB backup RAM is large enough for the 4 KiB virtual SmartEEPROM plus a transaction header and CRC. However, [`pre_startup()` currently clears it on an NVM reset (line 114)](/home/stem/projects/atsamd51/dropalt/board-dropalt/board.c:114), so the update partition would need to survive BKSWRST.
* Backup RAM is not battery-backed on this board. A power loss during the transaction can lose the snapshot. Firmware recovery should remain possible, but user settings may be lost. Fully power-fail-safe persistence would require host-side backup, external storage, or some permanently reserved flash.
* The DFU path needs an explicit size check. RIOT’s [`riotboot_flashwrite_putbytes()` (line 106)](/home/stem/projects/atsamd51/dropalt/riot/sys/riotboot/flashwrite.c:106) does not enforce SLOT0_LEN; it advances and erases pages until the physical-flash assertion eventually trips.
* USER-page endurance should be considered because each firmware update would perform two configuration rewrites.

[_dtoa_r() and _printf_float()]
* In this newlib-nano archive, _printf_float is a weak symbol. The integer formatter checks whether it is present; defining a strong _printf_float in our code would activate float conversions without linking newlib’s nano-vfprintf_float.o or _dtoa.

However, _printf_float is a private newlib ABI hook. It receives internal formatter state, parsed flags/width/precision, an output callback, and the va_list. lua_user_number2str(float) has none of that interface; it only converts one float to a compact string.

* Newlib’s _printf_float calls _dtoa_r (not lua_user_number2str()). _dtoa_r is the expensive part:
Component                   .text size
_printf_float plus helpers	2,828 bytes
_dtoa_r alone               6,488 bytes
And _dtoa_r pulls further multiple-precision helpers and allocation support, so its total linked cost is larger still.

However, lua_user_number2str() cannot replace _dtoa_r directly. _dtoa_r has a very different private interface: it receives a double, conversion mode, requested digit count, and returns a raw digit sequence plus separate decimal-point/sign metadata; newlib’s _printf_float then applies %f/%e/%g, width, precision, padding, and so on.



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
