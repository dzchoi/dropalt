#pragma once



namespace lua {

// Load the "keymap" module from flash memory.
// Note: The module must manually assign a non-nil value to `package.loaded["keymap"]`
// and return its module table.
void load_keymap();

// C++ wrapper for the keymap driver in Lua. Dynamically dispatches firmware-level key
// input to user-defined key mapping logic.
void handle_key_event(unsigned slot_index, bool is_press);

// C++ wrapper for the keyboard indicator lamp driver in Lua.
void handle_lamp_state(uint_fast8_t lamp_state);

}
