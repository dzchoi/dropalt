### Lua references:
in Lua, references apply only to compound types like tables, functions, and userdata. Primitive types such as numbers, strings, and booleans are always passed and assigned by value, not by reference.

If a table has a key/value of compound type, the key/value actually contains a reference to the object, and does not contain the entire object. So, a reference to a table key/value does not make sense in rigorous terms, it is a reference to the object that the table key/value also references.

### Only objects are collectible, not values:
- Numbers, booleans, and nil are not subject to garbage collection. They are value types, not objects with memory references.
- In Lua, strings are interned, meaning they are stored in a global string table. This makes them always strongly referenced, so they won’t be collected, even in weak-key tables.

### Weak tables:
- Strings are interned → not collectible.
- Numbers are primitives → not collectible.
- So weak tables with only primitive keys/values won’t auto-clean.

### No global variables in Lua.
- There is no top-most level scope in Lua. In Lua, everything is a chunk, whether it's a file, a string passed to load, or code entered interactively.
- Every function that accesses a “global” variable will have an upvalue for _ENV, which points to _G unless explicitly overridden.
- In Lua 5.2 and later, global variable access is syntactic sugar for _ENV["name"].

### Using a weak table for _ENV
- A weak table can be given as _ENV to a chunk.
  ```
    ( -- chunk )
    lua_newtable(L);                // Create a new local environment.
    lua_newtable(L);                // Create its metatable.
    lua_pushglobaltable(L);         // Push _G
    lua_setfield(L, -2, "__index"); // metatable.__index = _G
    lua_pushstring(L, "v");         // Value references are weak.
    lua_setfield(L, -2, "__mode");  // Make the environment a weak table.
    lua_setmetatable(L, -2);        // setmetatable(env, metatable)
    // ( -- chunk environment )

    lua_setupvalue(L, -2, 1);       // Set the _ENV upvalue to the new environment.
    // ( -- chunk )
  ```
- This weak table can help automatically remove unreferenced objects during garbage collection, though its effectiveness is limited:
  * Entries with only primitive keys or values (numbers, strings, booleans) are not removed, whether or not they are referenced, because primitive types are not collectible.
  * Referencing an external local variable (even without assigning it) causes the variable to be captured as an upvalue, storing either the primitive value directly or a reference to the object."
  * Referencing a global variable adds only _ENV as an upvalue, not the global variable itself.
  * Be cautious to ensure that any objects you intend to keep are properly referenced. For example, circular references will still be collected unless at least one object in the cycle is reachable from outside.
