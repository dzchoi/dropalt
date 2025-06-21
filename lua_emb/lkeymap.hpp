#pragma once



namespace lua {

// Load the "keymap" module from flash memory.
// Note: The module must insert its own table into package.loaded[] and return it.
void load_keymap();

// Keymap engine that triggers all on_*_press/release() callbacks.
void handle_key_event(unsigned slot_index, bool is_press);

}
