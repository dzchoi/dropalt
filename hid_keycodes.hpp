// These keycodes are based on Universal Serial Bus HID Usage Tables Document, v1.12.
// Chapter 10: Keyboard/Keypad Page(0x07) - Page 53
// https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf

#pragma once

#include <type_traits>          // for std::integral_constant<>



template <uint8_t CODE>
using Key = std::integral_constant<uint8_t, CODE>;

// USB HID Keyboard/Keypad Usage(0x07)
constexpr Key<0x00> KC_NO;
constexpr Key<0x01> KC_ROLL_OVER;  // equals KC_TRANSPARENT
constexpr Key<0x02> KC_POST_FAIL;
constexpr Key<0x03> KC_UNDEFINED;
constexpr Key<0x04> KC_A;
constexpr Key<0x05> KC_B;
constexpr Key<0x06> KC_C;
constexpr Key<0x07> KC_D;
constexpr Key<0x08> KC_E;
constexpr Key<0x09> KC_F;
constexpr Key<0x0A> KC_G;
constexpr Key<0x0B> KC_H;
constexpr Key<0x0C> KC_I;
constexpr Key<0x0D> KC_J;
constexpr Key<0x0E> KC_K;
constexpr Key<0x0F> KC_L;

constexpr Key<0x10> KC_M;
constexpr Key<0x11> KC_N;
constexpr Key<0x12> KC_O;
constexpr Key<0x13> KC_P;
constexpr Key<0x14> KC_Q;
constexpr Key<0x15> KC_R;
constexpr Key<0x16> KC_S;
constexpr Key<0x17> KC_T;
constexpr Key<0x18> KC_U;
constexpr Key<0x19> KC_V;
constexpr Key<0x1A> KC_W;
constexpr Key<0x1B> KC_X;
constexpr Key<0x1C> KC_Y;
constexpr Key<0x1D> KC_Z;
constexpr Key<0x1E> KC_1;
constexpr Key<0x1F> KC_2;

constexpr Key<0x20> KC_3;
constexpr Key<0x21> KC_4;
constexpr Key<0x22> KC_5;
constexpr Key<0x23> KC_6;
constexpr Key<0x24> KC_7;
constexpr Key<0x25> KC_8;
constexpr Key<0x26> KC_9;
constexpr Key<0x27> KC_0;
constexpr Key<0x28> KC_ENTER;
constexpr Key<0x29> KC_ESCAPE;
constexpr Key<0x2A> KC_BSPACE;
constexpr Key<0x2B> KC_TAB;
constexpr Key<0x2C> KC_SPACE;
constexpr Key<0x2D> KC_MINUS;
constexpr Key<0x2E> KC_EQUAL;
constexpr Key<0x2F> KC_LBRACKET;

constexpr Key<0x30> KC_RBRACKET;
constexpr Key<0x31> KC_BSLASH;      // \ (and |)
constexpr Key<0x32> KC_NONUS_HASH;  // Non-US # and ~ (Typically near the Enter key)
constexpr Key<0x33> KC_SCOLON;      // ; (and :)
constexpr Key<0x34> KC_QUOTE;       // ' and "
constexpr Key<0x35> KC_GRAVE;       // Grave accent and tilde
constexpr Key<0x36> KC_COMMA;       // , and <
constexpr Key<0x37> KC_DOT;         // . and >
constexpr Key<0x38> KC_SLASH;       // / and ?
constexpr Key<0x39> KC_CAPSLOCK;
constexpr Key<0x3A> KC_F1;
constexpr Key<0x3B> KC_F2;
constexpr Key<0x3C> KC_F3;
constexpr Key<0x3D> KC_F4;
constexpr Key<0x3E> KC_F5;
constexpr Key<0x3F> KC_F6;

constexpr Key<0x40> KC_F7;
constexpr Key<0x41> KC_F8;
constexpr Key<0x42> KC_F9;
constexpr Key<0x43> KC_F10;
constexpr Key<0x44> KC_F11;
constexpr Key<0x45> KC_F12;
constexpr Key<0x46> KC_PSCREEN;
constexpr Key<0x47> KC_SCROLLLOCK;
constexpr Key<0x48> KC_PAUSE;
constexpr Key<0x49> KC_INSERT;
constexpr Key<0x4A> KC_HOME;
constexpr Key<0x4B> KC_PGUP;
constexpr Key<0x4C> KC_DELETE;
constexpr Key<0x4D> KC_END;
constexpr Key<0x4E> KC_PGDOWN;
constexpr Key<0x4F> KC_RIGHT;

constexpr Key<0x50> KC_LEFT;
constexpr Key<0x51> KC_DOWN;
constexpr Key<0x52> KC_UP;
constexpr Key<0x53> KC_NUMLOCK;
constexpr Key<0x54> KC_KP_SLASH;
constexpr Key<0x55> KC_KP_ASTERISK;
constexpr Key<0x56> KC_KP_MINUS;
constexpr Key<0x57> KC_KP_PLUS;
constexpr Key<0x58> KC_KP_ENTER;
constexpr Key<0x59> KC_KP_1;
constexpr Key<0x5A> KC_KP_2;
constexpr Key<0x5B> KC_KP_3;
constexpr Key<0x5C> KC_KP_4;
constexpr Key<0x5D> KC_KP_5;
constexpr Key<0x5E> KC_KP_6;
constexpr Key<0x5F> KC_KP_7;

constexpr Key<0x60> KC_KP_8;
constexpr Key<0x61> KC_KP_9;
constexpr Key<0x62> KC_KP_0;
constexpr Key<0x63> KC_KP_DOT;
constexpr Key<0x64> KC_NONUS_BSLASH;  // Non-US \ and | (Typically near the Left-Shift key)
constexpr Key<0x65> KC_APPLICATION;
constexpr Key<0x66> KC_POWER;
constexpr Key<0x67> KC_KP_EQUAL;
constexpr Key<0x68> KC_F13;
constexpr Key<0x69> KC_F14;
constexpr Key<0x6A> KC_F15;
constexpr Key<0x6B> KC_F16;
constexpr Key<0x6C> KC_F17;
constexpr Key<0x6D> KC_F18;
constexpr Key<0x6E> KC_F19;
constexpr Key<0x6F> KC_F20;

constexpr Key<0x70> KC_F21;
constexpr Key<0x71> KC_F22;
constexpr Key<0x72> KC_F23;
constexpr Key<0x73> KC_F24;
constexpr Key<0x74> KC_EXECUTE;
constexpr Key<0x75> KC_HELP;
constexpr Key<0x76> KC_MENU;
constexpr Key<0x77> KC_SELECT;
constexpr Key<0x78> KC_STOP;
constexpr Key<0x79> KC_AGAIN;
constexpr Key<0x7A> KC_UNDO;
constexpr Key<0x7B> KC_CUT;
constexpr Key<0x7C> KC_COPY;
constexpr Key<0x7D> KC_PASTE;
constexpr Key<0x7E> KC_FIND;
constexpr Key<0x7F> KC_MUTE;

constexpr Key<0x80> KC_VOLUP;
constexpr Key<0x81> KC_VOLDOWN;
constexpr Key<0x82> KC_LOCKING_CAPS;    // locking Caps Lock
constexpr Key<0x83> KC_LOCKING_NUM;     // locking Num Lock
constexpr Key<0x84> KC_LOCKING_SCROLL;  // locking Scroll Lock
constexpr Key<0x85> KC_KP_COMMA;
constexpr Key<0x86> KC_KP_EQUAL_AS400;  // equal sign on AS/400
constexpr Key<0x87> KC_INT1;
constexpr Key<0x88> KC_INT2;
constexpr Key<0x89> KC_INT3;
constexpr Key<0x8A> KC_INT4;
constexpr Key<0x8B> KC_INT5;
constexpr Key<0x8C> KC_INT6;
constexpr Key<0x8D> KC_INT7;
constexpr Key<0x8E> KC_INT8;
constexpr Key<0x8F> KC_INT9;

constexpr Key<0x90> KC_LANG1;
constexpr Key<0x91> KC_LANG2;
constexpr Key<0x92> KC_LANG3;
constexpr Key<0x93> KC_LANG4;
constexpr Key<0x94> KC_LANG5;
constexpr Key<0x95> KC_LANG6;
constexpr Key<0x96> KC_LANG7;
constexpr Key<0x97> KC_LANG8;
constexpr Key<0x98> KC_LANG9;
constexpr Key<0x99> KC_ALT_ERASE;
constexpr Key<0x9A> KC_SYSREQ;
constexpr Key<0x9B> KC_CANCEL;
constexpr Key<0x9C> KC_CLEAR;
constexpr Key<0x9D> KC_PRIOR;
constexpr Key<0x9E> KC_RETURN;
constexpr Key<0x9F> KC_SEPARATOR;

constexpr Key<0xA0> KC_OUT;
constexpr Key<0xA1> KC_OPER;
constexpr Key<0xA2> KC_CLEAR_AGAIN;
constexpr Key<0xA3> KC_CRSEL;
constexpr Key<0xA4> KC_EXSEL;
constexpr Key<0xA5> KC_RESERVED_A5;
constexpr Key<0xA6> KC_RESERVED_A6;
constexpr Key<0xA7> KC_RESERVED_A7;
constexpr Key<0xA8> KC_RESERVED_A8;
constexpr Key<0xA9> KC_RESERVED_A9;
constexpr Key<0xAA> KC_RESERVED_AA;
constexpr Key<0xAB> KC_RESERVED_AB;
constexpr Key<0xAC> KC_RESERVED_AC;
constexpr Key<0xAD> KC_RESERVED_AD;
constexpr Key<0xAE> KC_RESERVED_AE;
constexpr Key<0xAF> KC_RESERVED_AF;

// NOTE: Following codes(0xB0-DD) are not used but are in the HID Document. Leaving them for reference.
constexpr Key<0xB0> KC_KP_00;
constexpr Key<0xB1> KC_KP_000;
constexpr Key<0xB2> KC_THOUSANDS_SEPARATOR;
constexpr Key<0xB3> KC_DECIMAL_SEPARATOR;
constexpr Key<0xB4> KC_CURRENCY_UNIT;
constexpr Key<0xB5> KC_CURRENCY_SUB_UNIT;
constexpr Key<0xB6> KC_KP_LPAREN;
constexpr Key<0xB7> KC_KP_RPAREN;
constexpr Key<0xB8> KC_KP_LCBRACKET;  // {
constexpr Key<0xB9> KC_KP_RCBRACKET;  // }
constexpr Key<0xBA> KC_KP_TAB;
constexpr Key<0xBB> KC_KP_BSPACE;
constexpr Key<0xBC> KC_KP_A;
constexpr Key<0xBD> KC_KP_B;
constexpr Key<0xBE> KC_KP_C;
constexpr Key<0xBF> KC_KP_D;

constexpr Key<0xC0> KC_KP_E;
constexpr Key<0xC1> KC_KP_F;
constexpr Key<0xC2> KC_KP_XOR;
constexpr Key<0xC3> KC_KP_HAT;
constexpr Key<0xC4> KC_KP_PERC;
constexpr Key<0xC5> KC_KP_LT;
constexpr Key<0xC6> KC_KP_GT;
constexpr Key<0xC7> KC_KP_AND;
constexpr Key<0xC8> KC_KP_LAZYAND;
constexpr Key<0xC9> KC_KP_OR;
constexpr Key<0xCA> KC_KP_LAZYOR;
constexpr Key<0xCB> KC_KP_COLON;
constexpr Key<0xCC> KC_KP_HASH;
constexpr Key<0xCD> KC_KP_SPACE;
constexpr Key<0xCE> KC_KP_ATMARK;
constexpr Key<0xCF> KC_KP_EXCLAMATION;

constexpr Key<0xD0> KC_KP_MEM_STORE;
constexpr Key<0xD1> KC_KP_MEM_RECALL;
constexpr Key<0xD2> KC_KP_MEM_CLEAR;
constexpr Key<0xD3> KC_KP_MEM_ADD;
constexpr Key<0xD4> KC_KP_MEM_SUB;
constexpr Key<0xD5> KC_KP_MEM_MUL;
constexpr Key<0xD6> KC_KP_MEM_DIV;
constexpr Key<0xD7> KC_KP_PLUS_MINUS;
constexpr Key<0xD8> KC_KP_CLEAR;
constexpr Key<0xD9> KC_KP_CLEAR_ENTRY;
constexpr Key<0xDA> KC_KP_BINARY;
constexpr Key<0xDB> KC_KP_OCTAL;
constexpr Key<0xDC> KC_KP_DECIMAL;
constexpr Key<0xDD> KC_KP_HEXADECIMAL;
// constexpr Key<0xDE> KC_???;
// constexpr Key<0xDF> KC_???;

// Modifiers
constexpr Key<0xE0> KC_LCTRL;
constexpr Key<0xE1> KC_LSHIFT;
constexpr Key<0xE2> KC_LALT;
constexpr Key<0xE3> KC_LGUI;
constexpr Key<0xE4> KC_RCTRL;
constexpr Key<0xE5> KC_RSHIFT;
constexpr Key<0xE6> KC_RALT;
constexpr Key<0xE7> KC_RGUI;


/*
 * Short names for ease of definition of keymap
 */
constexpr auto KC_LCTL = KC_LCTRL;
constexpr auto KC_RCTL = KC_RCTRL;
constexpr auto KC_LSFT = KC_LSHIFT;
constexpr auto KC_RSFT = KC_RSHIFT;
constexpr auto KC_ESC  = KC_ESCAPE;
constexpr auto KC_BSPC = KC_BSPACE;
constexpr auto KC_ENT  = KC_ENTER;
constexpr auto KC_DEL  = KC_DELETE;
constexpr auto KC_INS  = KC_INSERT;
constexpr auto KC_CAPS = KC_CAPSLOCK;
constexpr auto KC_CLCK = KC_CAPSLOCK;
constexpr auto KC_RGHT = KC_RIGHT;
constexpr auto KC_PGDN = KC_PGDOWN;
constexpr auto KC_PSCR = KC_PSCREEN;
constexpr auto KC_SLCK = KC_SCROLLLOCK;
constexpr auto KC_PAUS = KC_PAUSE;
constexpr auto KC_BRK  = KC_PAUSE;
constexpr auto KC_NLCK = KC_NUMLOCK;
constexpr auto KC_SPC  = KC_SPACE;
constexpr auto KC_MINS = KC_MINUS;
constexpr auto KC_EQL  = KC_EQUAL;
constexpr auto KC_GRV  = KC_GRAVE;
constexpr auto KC_RBRC = KC_RBRACKET;
constexpr auto KC_LBRC = KC_LBRACKET;
constexpr auto KC_COMM = KC_COMMA;
constexpr auto KC_BSLS = KC_BSLASH;
constexpr auto KC_SLSH = KC_SLASH;
constexpr auto KC_SCLN = KC_SCOLON;
constexpr auto KC_QUOT = KC_QUOTE;
constexpr auto KC_APP  = KC_APPLICATION;
constexpr auto KC_NUHS = KC_NONUS_HASH;
constexpr auto KC_NUBS = KC_NONUS_BSLASH;
constexpr auto KC_LCAP = KC_LOCKING_CAPS;
constexpr auto KC_LNUM = KC_LOCKING_NUM;
constexpr auto KC_LSCR = KC_LOCKING_SCROLL;
constexpr auto KC_ERAS = KC_ALT_ERASE;
constexpr auto KC_CLR  = KC_CLEAR;
/* Japanese specific */
constexpr auto KC_ZKHK = KC_GRAVE;
constexpr auto KC_RO   = KC_INT1;
constexpr auto KC_KANA = KC_INT2;
constexpr auto KC_JYEN = KC_INT3;
constexpr auto KC_HENK = KC_INT4;
constexpr auto KC_MHEN = KC_INT5;
/* Korean specific */
constexpr auto KC_HAEN = KC_LANG1;
constexpr auto KC_HANJ = KC_LANG2;
/* Keypad */
constexpr auto KC_P1   = KC_KP_1;
constexpr auto KC_P2   = KC_KP_2;
constexpr auto KC_P3   = KC_KP_3;
constexpr auto KC_P4   = KC_KP_4;
constexpr auto KC_P5   = KC_KP_5;
constexpr auto KC_P6   = KC_KP_6;
constexpr auto KC_P7   = KC_KP_7;
constexpr auto KC_P8   = KC_KP_8;
constexpr auto KC_P9   = KC_KP_9;
constexpr auto KC_P0   = KC_KP_0;
constexpr auto KC_PDOT = KC_KP_DOT;
constexpr auto KC_PCMM = KC_KP_COMMA;
constexpr auto KC_PSLS = KC_KP_SLASH;
constexpr auto KC_PAST = KC_KP_ASTERISK;
constexpr auto KC_PMNS = KC_KP_MINUS;
constexpr auto KC_PPLS = KC_KP_PLUS;
constexpr auto KC_PEQL = KC_KP_EQUAL;
constexpr auto KC_PENT = KC_KP_ENTER;
/* Unix function key */
constexpr auto KC_EXEC = KC_EXECUTE;
constexpr auto KC_SLCT = KC_SELECT;
constexpr auto KC_AGIN = KC_AGAIN;
constexpr auto KC_PSTE = KC_PASTE;
/* Mousekey */
// constexpr auto KC_MS_U = KC_MS_UP;
// constexpr auto KC_MS_D = KC_MS_DOWN;
// constexpr auto KC_MS_L = KC_MS_LEFT;
// constexpr auto KC_MS_R = KC_MS_RIGHT;
// constexpr auto KC_BTN1 = KC_MS_BTN1;
// constexpr auto KC_BTN2 = KC_MS_BTN2;
// constexpr auto KC_BTN3 = KC_MS_BTN3;
// constexpr auto KC_BTN4 = KC_MS_BTN4;
// constexpr auto KC_BTN5 = KC_MS_BTN5;
// constexpr auto KC_WH_U = KC_MS_WH_UP;
// constexpr auto KC_WH_D = KC_MS_WH_DOWN;
// constexpr auto KC_WH_L = KC_MS_WH_LEFT;
// constexpr auto KC_WH_R = KC_MS_WH_RIGHT;
// constexpr auto KC_ACL0 = KC_MS_ACCEL0;
// constexpr auto KC_ACL1 = KC_MS_ACCEL1;
// constexpr auto KC_ACL2 = KC_MS_ACCEL2;

/* Transparent */
constexpr auto KC_TRANSPARENT = KC_ROLL_OVER;
constexpr auto KC_TRNS = KC_TRANSPARENT;
/* GUI key aliases */
constexpr auto KC_LCMD = KC_LGUI;
constexpr auto KC_LWIN = KC_LGUI;
constexpr auto KC_RCMD = KC_RGUI;
constexpr auto KC_RWIN = KC_RGUI;

// constexpr auto KC_LABK = KC_LT;
// constexpr auto KC_RABK = KC_GT;



template <uint8_t CODE>
constexpr uint8_t keycode(Key<CODE> key) {
    (void)key;
    return CODE;
}
