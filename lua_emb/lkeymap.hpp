#pragma once



namespace lua {

// Load the "keymap" module from flash memory and store the keymap and lamp drivers in
// the Lua registry.
// Note: The module must manually assign a non-nil value to `package.loaded["keymap"]`
// and return a table containing both the keymap and lamp drivers.
void load_keymap();

// C++ wrapper for the Lua-based keymap driver. Dispatches key input events from
// firmware to user-defined mapping logic in Lua.
void handle_key_event(unsigned slot_index, bool is_press);

// C++ wrapper for the Lua-based lamp driver. Updates keyboard indicator lamps based on
// user-defined logic in Lua.
void handle_lamp_state(uint_fast8_t lamp_state);

}
