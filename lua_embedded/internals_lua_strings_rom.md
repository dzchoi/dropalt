Lua strings are already immutable, so the "copy and modify" semantics are free (any `..`, `string.sub()`, `string.gsub()`, `string.format()`, etc. already produces a new `TString`), The obstacle isn't mutability; it's string interning.

Here's what makes this different from `Proto.code` / `Proto.lineinfo`:

#### Why `TString` can't just live in flash the way an `Instruction[]` can:

1. **Interning invariant.** Short strings (< LUAI_MAXSHORTLEN = 40, which is almost every string in our keymap: "on_press", "m_press_count", "TapHold [%d]", etc) are canonicalized - every occurrence of the same content must have the same `TString*` pointer. The VM exploits this via `eqshrstr(a, b)` = pointer compare. Two independent flash `TString`s with the same content would break `==`.

2. **GC writes to the header.** `TString` shares CommonHeader (`next`, `tt`, `marked`); the collector rewrites `marked` on every cycle. A flash-resident header would fault (or, worse, silently no-op with a read-modify-write to flash-mapped registers). This is fixable by marking such objects `LUAGCFIXED` so the collector skips them, but it means every `TString` write path must first check.

3. **Layout coupling.** `getstr(ts) = (char*)(ts + 1)` - Lua assumes the payload is contiguous with the header. So the header + bytes have to live together in flash, and daluac has to emit them in an ABI-compatible layout for our specific `LUAI_MAXSHORTLEN`, `l_uint32`, etc.

4. **strt is in RAM.** The interning hash table `global_State.strt` is a RAM structure; to make `luaS_newlstr("on_press", 8)` return a flash pointer, either the strt buckets have to link into flash entries or the loader has to seed strt with the flash entries at boot.

#### What "doing it right" would look like (the eLua "LTR" approach):

* daluac collects all unique string literals from the chunk, allocates each a properly-aligned `TString` header + payload in the bytecode image, precomputes their hash, sets `marked = LUAGCFIXED`, and threads them into hash-bucket chains via `hnext` pointers relative to the image base.

* Bytecode format grows a "ROM string table" section right after the header (needs `LUAC_FORMAT` bump).

* Firmware at `luaU_undump` time walks that table and links each entry into `global_State.strt`. From then on `luaS_newlstr("on_press")` naturally finds and returns the flash pointer.

* `LoadConstants` reads a string constant as an index into the ROM string table (or a NULL-terminated inline string that gets interned in RAM as fallback).

* The GC's mark/sweep paths test `LUAGCFIXED` before writing `marked` or freeing.

* `Proto.k` (`f->k`, constant pool) - the `TValue` itself is fixed at load time and can live in flash.  
  `TValue` is 8 bytes with alignment, and string constants embed pointers to interned `TString` objects. Also requires numeric constants in flash.

Roughly 200-300 lines of changes across lstring.c, lgc.c, lundump.c on the firmware side + the ROM table emitter and format bump in daluac.

#### Expected saving in our case:

Rough estimate for user_logic/ - probably ~80-120 unique short strings averaging ~12 chars = ~1.5-2.5 KB of `TString` payload. Add the ~16-byte header per unique string = another 1.3-1.9 KB. So call it 3-4 KB total, and even that only applies to retained strings (transient ones get GC'd whether interned in RAM or in flash).
