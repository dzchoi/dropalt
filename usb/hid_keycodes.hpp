// These keycodes are based on Universal Serial Bus HID Usage Tables Document, v1.12.
// Chapter 10: Keyboard/Keypad Page(0x07) - Page 53
// https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf

#pragma once

#include <cstdint>              // for uint8_t


// USB HID Keyboard/Keypad Usage (0x07)
// Note: As a constexpr, this ~1KB array is only included in the binary if referenced at
// runtime.
inline constexpr const char* keycode_to_name[] = {
    "NULL",             // 0x00
    "ROLLOVER",
    "POSTFAIL",
    "UNDEFINED",
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",                // 0x10
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",
    "1",
    "2",
    "3",                // 0x20
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "0",
    "ENTER",
    "ESC",
    "BKSP",
    "TAB",
    "SPACE",
    "-",
    "=",
    "[",
    "]",                // 0x30
    "\\",               // \ (and |)
    "NONUS#",           // Non-US # and ~ (Typically near the Enter key)
    ";",                // ; (and :)
    "'",                // ' and "
    "`",                // Grave accent and tilde
    ",",                // , and <
    ".",                // . and >
    "/",                // / and ?
    "CAPSLOCK",
    "F1",
    "F2",
    "F3",
    "F4",
    "F5",
    "F6",
    "F7",               // 0x40
    "F8",
    "F9",
    "F10",
    "F11",
    "F12",
    "PRTSCR",
    "SCRLOCK",
    "PAUSE",
    "INS",
    "HOME",
    "PGUP",
    "DEL",
    "END",
    "PGDN",
    "RIGHT",
    "LEFT",             // 0x50
    "DOWN",
    "UP",
    "NUMLOCK",
    "P/",
    "P*",
    "P-",
    "P+",
    "PENTER",
    "P1",
    "P2",
    "P3",
    "P4",
    "P5",
    "P6",
    "P7",
    "P8",               // 0x60
    "P9",
    "P0",
    "P.",
    "NONUS\\",          // Non-US \ and | (Typically near the Left-Shift key)
    "APP",
    "POWER",
    "P=",
    "F13",
    "F14",
    "F15",
    "F16",
    "F17",
    "F18",
    "F19",
    "F20",
    "F21",              // 0x70
    "F22",
    "F23",
    "F24",
    "EXECUTE",
    "HELP",
    "MENU",
    "SELECT",
    "STOP",
    "AGAIN",
    "UNDO",
    "CUT",
    "COPY",
    "PASTE",
    "FIND",
    "MUTE",
    "VOLUP",            // 0x80
    "VOLDN",
    "LOCKINGCAPS",      // locking Caps Lock
    "LOCKINGNUM",       // locking Num Lock
    "LOCKINGSCR",       // locking Scroll Lock
    "P,",
    "P=",               // equal sign on AS/400
    "INT1",
    "INT2",
    "INT3",
    "INT4",
    "INT5",
    "INT6",
    "INT7",
    "INT8",
    "INT9",
    "LANG1",            // 0x90
    "LANG2",
    "LANG3",
    "LANG4",
    "LANG5",
    "LANG6",
    "LANG7",
    "LANG8",
    "LANG9",
    "ALTERASE",
    "SYSREQ",
    "CANCEL",
    "CLEAR",
    "PRIOR",
    "RETURN",
    "SEPARATOR",
    "OUT",              // 0xA0
    "OPER",
    "CLEARAGAIN",
    "CRSEL",
    "EXSEL",            // 0xA4

    // 0xA5 to 0xAF - RESERVED
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",

    // NOTE: The following codes (0xB0–0xDD) are defined in the HID specification (e.g.
    // Keypad 00) but are not used.
    "NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA",  // 0xB0
    "NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA",  // 0xC0
    "NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA","NA",  // 0xD0

    // Modifiers
    "LCTRL",            // 0xE0
    "LSHIFT",
    "LALT",
    "LGUI",
    "RCTRL",
    "RSHIFT",
    "RALT",
    "RGUI",
};


// Shortcut keycodes
constexpr uint8_t KC_NO = 0;
constexpr uint8_t KC_A = 4;


// Note: This uses a simple linear search to map the key name to its keycode.
constexpr uint8_t keycode(const char* keyname)
{
    constexpr uint8_t NUM_KEYCODES =
        sizeof(keycode_to_name) / sizeof(keycode_to_name[0]);

    uint8_t result = KC_NO;  // KC_NO (= 0) is not a valid keycode.
    for ( uint8_t keycode = KC_A ; keycode < NUM_KEYCODES ; keycode++ )
        if ( __builtin_strcmp(keyname, keycode_to_name[keycode]) == 0 ) {
            result = keycode;
            break;
        }

    return result;
}


// More shortcuts
constexpr uint8_t KC_LCTRL = keycode("LCTRL");
