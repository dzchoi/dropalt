// These keycodes are based on Universal Serial Bus HID Usage Tables Document, v1.12.
// Chapter 10: Keyboard/Keypad Page(0x07) - Page 53
// https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf

#pragma once



// USB HID Keyboard/Keypad Usage(0x07)
enum hid_keyboard_keypad_usage: uint8_t {
    KC_NO               = 0x00,
    KC_ROLL_OVER,       // this is equivalent to KC_TRANSPARENT
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

#if 0
    /. NOTE: Following codes(0xB0-DD) are not used but are in the HID Document. Leaving them for reference.
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
#endif

    /* Modifiers */
    KC_LCTRL            = 0xE0,
    KC_LSHIFT,
    KC_LALT,
    KC_LGUI,
    KC_RCTRL,
    KC_RSHIFT,
    KC_RALT,
    KC_RGUI,
};



/*
 * Short names for ease of definition of keymap
 */
constexpr uint8_t KC_LCTL = KC_LCTRL;
constexpr uint8_t KC_RCTL = KC_RCTRL;
constexpr uint8_t KC_LSFT = KC_LSHIFT;
constexpr uint8_t KC_RSFT = KC_RSHIFT;
constexpr uint8_t KC_ESC  = KC_ESCAPE;
constexpr uint8_t KC_BSPC = KC_BSPACE;
constexpr uint8_t KC_ENT  = KC_ENTER;
constexpr uint8_t KC_DEL  = KC_DELETE;
constexpr uint8_t KC_INS  = KC_INSERT;
constexpr uint8_t KC_CAPS = KC_CAPSLOCK;
constexpr uint8_t KC_CLCK = KC_CAPSLOCK;
constexpr uint8_t KC_RGHT = KC_RIGHT;
constexpr uint8_t KC_PGDN = KC_PGDOWN;
constexpr uint8_t KC_PSCR = KC_PSCREEN;
constexpr uint8_t KC_SLCK = KC_SCROLLLOCK;
constexpr uint8_t KC_PAUS = KC_PAUSE;
constexpr uint8_t KC_BRK  = KC_PAUSE;
constexpr uint8_t KC_NLCK = KC_NUMLOCK;
constexpr uint8_t KC_SPC  = KC_SPACE;
constexpr uint8_t KC_MINS = KC_MINUS;
constexpr uint8_t KC_EQL  = KC_EQUAL;
constexpr uint8_t KC_GRV  = KC_GRAVE;
constexpr uint8_t KC_RBRC = KC_RBRACKET;
constexpr uint8_t KC_LBRC = KC_LBRACKET;
constexpr uint8_t KC_COMM = KC_COMMA;
constexpr uint8_t KC_BSLS = KC_BSLASH;
constexpr uint8_t KC_SLSH = KC_SLASH;
constexpr uint8_t KC_SCLN = KC_SCOLON;
constexpr uint8_t KC_QUOT = KC_QUOTE;
constexpr uint8_t KC_APP  = KC_APPLICATION;
constexpr uint8_t KC_NUHS = KC_NONUS_HASH;
constexpr uint8_t KC_NUBS = KC_NONUS_BSLASH;
constexpr uint8_t KC_LCAP = KC_LOCKING_CAPS;
constexpr uint8_t KC_LNUM = KC_LOCKING_NUM;
constexpr uint8_t KC_LSCR = KC_LOCKING_SCROLL;
constexpr uint8_t KC_ERAS = KC_ALT_ERASE;
constexpr uint8_t KC_CLR  = KC_CLEAR;
/* Japanese specific */
constexpr uint8_t KC_ZKHK = KC_GRAVE;
constexpr uint8_t KC_RO   = KC_INT1;
constexpr uint8_t KC_KANA = KC_INT2;
constexpr uint8_t KC_JYEN = KC_INT3;
constexpr uint8_t KC_HENK = KC_INT4;
constexpr uint8_t KC_MHEN = KC_INT5;
/* Korean specific */
constexpr uint8_t KC_HAEN = KC_LANG1;
constexpr uint8_t KC_HANJ = KC_LANG2;
/* Keypad */
constexpr uint8_t KC_P1   = KC_KP_1;
constexpr uint8_t KC_P2   = KC_KP_2;
constexpr uint8_t KC_P3   = KC_KP_3;
constexpr uint8_t KC_P4   = KC_KP_4;
constexpr uint8_t KC_P5   = KC_KP_5;
constexpr uint8_t KC_P6   = KC_KP_6;
constexpr uint8_t KC_P7   = KC_KP_7;
constexpr uint8_t KC_P8   = KC_KP_8;
constexpr uint8_t KC_P9   = KC_KP_9;
constexpr uint8_t KC_P0   = KC_KP_0;
constexpr uint8_t KC_PDOT = KC_KP_DOT;
constexpr uint8_t KC_PCMM = KC_KP_COMMA;
constexpr uint8_t KC_PSLS = KC_KP_SLASH;
constexpr uint8_t KC_PAST = KC_KP_ASTERISK;
constexpr uint8_t KC_PMNS = KC_KP_MINUS;
constexpr uint8_t KC_PPLS = KC_KP_PLUS;
constexpr uint8_t KC_PEQL = KC_KP_EQUAL;
constexpr uint8_t KC_PENT = KC_KP_ENTER;
/* Unix function key */
constexpr uint8_t KC_EXEC = KC_EXECUTE;
constexpr uint8_t KC_SLCT = KC_SELECT;
constexpr uint8_t KC_AGIN = KC_AGAIN;
constexpr uint8_t KC_PSTE = KC_PASTE;
/* Mousekey */
// constexpr uint8_t KC_MS_U = KC_MS_UP;
// constexpr uint8_t KC_MS_D = KC_MS_DOWN;
// constexpr uint8_t KC_MS_L = KC_MS_LEFT;
// constexpr uint8_t KC_MS_R = KC_MS_RIGHT;
// constexpr uint8_t KC_BTN1 = KC_MS_BTN1;
// constexpr uint8_t KC_BTN2 = KC_MS_BTN2;
// constexpr uint8_t KC_BTN3 = KC_MS_BTN3;
// constexpr uint8_t KC_BTN4 = KC_MS_BTN4;
// constexpr uint8_t KC_BTN5 = KC_MS_BTN5;
// constexpr uint8_t KC_WH_U = KC_MS_WH_UP;
// constexpr uint8_t KC_WH_D = KC_MS_WH_DOWN;
// constexpr uint8_t KC_WH_L = KC_MS_WH_LEFT;
// constexpr uint8_t KC_WH_R = KC_MS_WH_RIGHT;
// constexpr uint8_t KC_ACL0 = KC_MS_ACCEL0;
// constexpr uint8_t KC_ACL1 = KC_MS_ACCEL1;
// constexpr uint8_t KC_ACL2 = KC_MS_ACCEL2;

/* Transparent */
constexpr uint8_t KC_TRANSPARENT = 1;
constexpr uint8_t KC_TRNS = KC_TRANSPARENT;
/* GUI key aliases */
constexpr uint8_t KC_LCMD = KC_LGUI;
constexpr uint8_t KC_LWIN = KC_LGUI;
constexpr uint8_t KC_RCMD = KC_RGUI;
constexpr uint8_t KC_RWIN = KC_RGUI;
constexpr uint8_t _______ = KC_TRNS;
constexpr uint8_t XXXXXXX = KC_NO;

// constexpr uint8_t KC_LABK = KC_LT;
// constexpr uint8_t KC_RABK = KC_GT;
