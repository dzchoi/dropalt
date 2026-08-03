The `Base.c_keymap_table` cannot stay in ROM; the entries are the problem, not the table object itself.

Each entry is an instance created by `ModIf()`, `Lit()`, `TapHold()`, etc. That instance carries mutable per-keymap state - fields like `m_press_count`, `m_is_modified`, `m_map_chosen`, `m_my_slot` are written on every relevant key event. So:

1. The keymap objects themselves must be in RAM (their bodies mutate).
2. Their addresses aren't known until load-time, so daluac cannot write them into a flash-resident table.
3. Consequently the table's `TValue[65]` array - which is just 65 pointers to those RAM objects - also has to be built at load time in RAM.

The table's own cost is modest anyway: sizeof(Table) header (~50 B) + `TValue[65]` array (65 * 8 = 520 B) = ~600 bytes. That's roughly what we'd save at most, and only if we could get it into flash.

#### Where a ROM-table optimization does work (the eLua "rotable" trick):

Tables whose entries are compile-time constants - string keys mapping to C functions, ints, or other ROM values. Standard library tables (`math.sin`, `string.format`, ...) are the canonical use case. That's not our keymap.
