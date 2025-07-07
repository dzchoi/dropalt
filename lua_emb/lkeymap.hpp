#pragma once



namespace lua {

// Load the "keymap" module from flash memory.
// Note: The module must insert its own table into package.loaded[] and return it.
void load_keymap();

// C++ counterpart to Lua’s Module.handle_key_event(); delegates key event handling to
// the core keymap engine in Lua.
void handle_key_event(unsigned slot_index1, bool is_press);

}
