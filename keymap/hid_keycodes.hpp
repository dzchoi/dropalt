// These keycodes are based on Universal Serial Bus HID Usage Tables Document, v1.12.
// Chapter 10: Keyboard/Keypad Page(0x07) - Page 53
// https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf

#pragma once

#include "stdint.h"             // for uint8_t



// USB HID Keyboard/Keypad Usage(0x07)

enum hid_keyboard_keypad_usage: uint8_t {
    KC_NO               = 0x00,
    KC_ROLL_OVER,
    KC_POST_FAIL,
    KC_UNDEFINED,
    KC_A,
    KC_B,
    KC_C,
    KC_D,
    KC_E,
    KC_F,
    KC_G,
    KC_H,
    KC_I,
    KC_J,
    KC_K,
    KC_L,
    KC_M,               // 0x10
    KC_N,
    KC_O,
    KC_P,
    KC_Q,
    KC_R,
    KC_S,
    KC_T,
    KC_U,
    KC_V,
    KC_W,
    KC_X,
    KC_Y,
    KC_Z,
    KC_1,
    KC_2,
    KC_3,               // 0x20
    KC_4,
    KC_5,
    KC_6,
    KC_7,
    KC_8,
    KC_9,
    KC_0,
    KC_ENTER,
    KC_ESCAPE,
    KC_BSPACE,
    KC_TAB,
    KC_SPACE,
    KC_MINUS,
    KC_EQUAL,
    KC_LBRACKET,
    KC_RBRACKET,        // 0x30
    KC_BSLASH,          // \ (and |)
    KC_NONUS_HASH,      // Non-US # and ~ (Typically near the Enter key)
    KC_SCOLON,          // ; (and :)
    KC_QUOTE,           // ' and "
    KC_GRAVE,           // Grave accent and tilde
    KC_COMMA,           // , and <
    KC_DOT,             // . and >
    KC_SLASH,           // / and ?
    KC_CAPSLOCK,
    KC_F1,
    KC_F2,
    KC_F3,
    KC_F4,
    KC_F5,
    KC_F6,
    KC_F7,              // 0x40
    KC_F8,
    KC_F9,
    KC_F10,
    KC_F11,
    KC_F12,
    KC_PSCREEN,
    KC_SCROLLLOCK,
    KC_PAUSE,
    KC_INSERT,
    KC_HOME,
    KC_PGUP,
    KC_DELETE,
    KC_END,
    KC_PGDOWN,
    KC_RIGHT,
    KC_LEFT,            // 0x50
    KC_DOWN,
    KC_UP,
    KC_NUMLOCK,
    KC_KP_SLASH,
    KC_KP_ASTERISK,
    KC_KP_MINUS,
    KC_KP_PLUS,
    KC_KP_ENTER,
    KC_KP_1,
    KC_KP_2,
    KC_KP_3,
    KC_KP_4,
    KC_KP_5,
    KC_KP_6,
    KC_KP_7,
    KC_KP_8,            // 0x60
    KC_KP_9,
    KC_KP_0,
    KC_KP_DOT,
    KC_NONUS_BSLASH,    // Non-US \ and | (Typically near the Left-Shift key)
    KC_APPLICATION,
    KC_POWER,
    KC_KP_EQUAL,
    KC_F13,
    KC_F14,
    KC_F15,
    KC_F16,
    KC_F17,
    KC_F18,
    KC_F19,
    KC_F20,
    KC_F21,             // 0x70
    KC_F22,
    KC_F23,
    KC_F24,
    KC_EXECUTE,
    KC_HELP,
    KC_MENU,
    KC_SELECT,
    KC_STOP,
    KC_AGAIN,
    KC_UNDO,
    KC_CUT,
    KC_COPY,
    KC_PASTE,
    KC_FIND,
    KC_MUTE,
    KC_VOLUP,           // 0x80
    KC_VOLDOWN,
    KC_LOCKING_CAPS,    // locking Caps Lock
    KC_LOCKING_NUM,     // locking Num Lock
    KC_LOCKING_SCROLL,  // locking Scroll Lock
    KC_KP_COMMA,
    KC_KP_EQUAL_AS400,  // equal sign on AS/400
    KC_INT1,
    KC_INT2,
    KC_INT3,
    KC_INT4,
    KC_INT5,
    KC_INT6,
    KC_INT7,
    KC_INT8,
    KC_INT9,
    KC_LANG1,           // 0x90
    KC_LANG2,
    KC_LANG3,
    KC_LANG4,
    KC_LANG5,
    KC_LANG6,
    KC_LANG7,
    KC_LANG8,
    KC_LANG9,
    KC_ALT_ERASE,
    KC_SYSREQ,
    KC_CANCEL,
    KC_CLEAR,
    KC_PRIOR,
    KC_RETURN,
    KC_SEPARATOR,
    KC_OUT,             // 0xA0
    KC_OPER,
    KC_CLEAR_AGAIN,
    KC_CRSEL,
    KC_EXSEL,           // 0xA4

    // 0xA5 to 0xAF - RESERVED
    KC_RESERVED_A5,     // Used as macro identifier
    KC_RESERVED_A6,     // this is used for special keyboard functions
    KC_RESERVED_A7,
    KC_RESERVED_A8,
    KC_RESERVED_A9,
    KC_RESERVED_AA,
    KC_RESERVED_AB,
    KC_RESERVED_AC,
    KC_RESERVED_AD,
    KC_RESERVED_AE,
    KC_RESERVED_AF,

    // NOTE: Following codes(0xB0-DD) are not used but are in the HID Document. Leaving them for reference.
    KC_KP_00            = 0xB0,
    KC_KP_000,
    KC_THOUSANDS_SEPARATOR,
    KC_DECIMAL_SEPARATOR,
    KC_CURRENCY_UNIT,
    KC_CURRENCY_SUB_UNIT,
    KC_KP_LPAREN, KC_KP_RPAREN,
    KC_KP_LCBRACKET,    // {
    KC_KP_RCBRACKET,    // }
    KC_KP_TAB,
    KC_KP_BSPACE,
    KC_KP_A,
    KC_KP_B,
    KC_KP_C,
    KC_KP_D,
    KC_KP_E,            // 0xC0
    KC_KP_F,
    KC_KP_XOR,
    KC_KP_HAT,
    KC_KP_PERC,
    KC_KP_LT,
    KC_KP_GT,
    KC_KP_AND,
    KC_KP_LAZYAND,
    KC_KP_OR,
    KC_KP_LAZYOR,
    KC_KP_COLON,
    KC_KP_HASH,
    KC_KP_SPACE,
    KC_KP_ATMARK,
    KC_KP_EXCLAMATION,
    KC_KP_MEM_STORE,    // 0xD0
    KC_KP_MEM_RECALL,
    KC_KP_MEM_CLEAR,
    KC_KP_MEM_ADD,
    KC_KP_MEM_SUB,
    KC_KP_MEM_MUL,
    KC_KP_MEM_DIV,
    KC_KP_PLUS_MINUS,
    KC_KP_CLEAR,
    KC_KP_CLEAR_ENTRY,
    KC_KP_BINARY,
    KC_KP_OCTAL,
    KC_KP_DECIMAL,
    KC_KP_HEXADECIMAL,  // 0xDD

    // Modifiers
    KC_LCTRL            = 0xE0,
    KC_LSHIFT,
    KC_LALT,
    KC_LGUI,
    KC_RCTRL,
    KC_RSHIFT,
    KC_RALT,
    KC_RGUI,
};
