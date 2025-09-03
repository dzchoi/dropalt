#pragma once



struct lua_State;

namespace lua {

// Enqueue the given function and the arguments into a call frame in the pending call
// list.
// fw.execute_later(f, ...) -> void
int execute_later(lua_State* L);

// Check whether the pending call list contains any entries.
// ( -- )
bool execute_is_pending();

// Execute all stored call frames in the pending call list (in reverse order), running
// each function with its arguments in a protected environment.
// ( -- )
void execute_pending_calls();

}
