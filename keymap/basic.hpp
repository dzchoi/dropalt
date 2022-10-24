#pragma once

#include "hid_keycodes.hpp"
#include "keymap.hpp"



namespace key {

class basic_t: public map_t {
public:
    constexpr basic_t(uint8_t keycode): m_code(keycode) {}

    void on_press(pmap_t*) { m_pressing = true; send_press(m_code); }

    void on_release(pmap_t*) { m_pressing = false; send_release(m_code); }

    bool is_pressing() const { return m_pressing; }

    uint8_t keycode() const { return m_code; }

private:
    const uint8_t m_code;
    bool m_pressing = false;
};



// Note that unused ones will not be allocated thanks to g++.

inline basic_t ROLL_OVER = KC_ROLL_OVER;
inline basic_t POST_FAIL = KC_POST_FAIL;
inline basic_t UNDEFINED = KC_UNDEFINED;
inline basic_t A = KC_A;
inline basic_t B = KC_B;
inline basic_t C = KC_C;
inline basic_t D = KC_D;
inline basic_t E = KC_E;
inline basic_t F = KC_F;
inline basic_t G = KC_G;
inline basic_t H = KC_H;
inline basic_t I = KC_I;
inline basic_t J = KC_J;
inline basic_t K = KC_K;
inline basic_t L = KC_L;

inline basic_t M = KC_M;                // 0x10
inline basic_t N = KC_N;
inline basic_t O = KC_O;
inline basic_t P = KC_P;
inline basic_t Q = KC_Q;
inline basic_t R = KC_R;
inline basic_t S = KC_S;
inline basic_t T = KC_T;
inline basic_t U = KC_U;
inline basic_t V = KC_V;
inline basic_t W = KC_W;
inline basic_t X = KC_X;
inline basic_t Y = KC_Y;
inline basic_t Z = KC_Z;
inline basic_t _1 = KC_1;
inline basic_t _2 = KC_2;

inline basic_t _3 = KC_3;               // 0x20
inline basic_t _4 = KC_4;
inline basic_t _5 = KC_5;
inline basic_t _6 = KC_6;
inline basic_t _7 = KC_7;
inline basic_t _8 = KC_8;
inline basic_t _9 = KC_9;
inline basic_t _0 = KC_0;
inline basic_t ENTER = KC_ENTER;
inline basic_t ESCAPE = KC_ESCAPE;
inline basic_t BSPACE = KC_BSPACE;
inline basic_t TAB = KC_TAB;
inline basic_t SPACE = KC_SPACE;
inline basic_t MINUS = KC_MINUS;
inline basic_t EQUAL = KC_EQUAL;
inline basic_t LBRACKET = KC_LBRACKET;

inline basic_t RBRACKET = KC_RBRACKET;  // 0x30
inline basic_t BSLASH = KC_BSLASH;          // \ (and |)
inline basic_t NONUS_HASH = KC_NONUS_HASH;  // Non-US # and ~
inline basic_t SCOLON = KC_SCOLON;          // ; (and :)
inline basic_t QUOTE = KC_QUOTE;            // ' and "
inline basic_t GRAVE = KC_GRAVE;            // Grave accent and tilde
inline basic_t COMMA = KC_COMMA;            // , and <
inline basic_t DOT = KC_DOT;                // . and >
inline basic_t SLASH = KC_SLASH;            // / and ?
inline basic_t CAPSLOCK = KC_CAPSLOCK;
inline basic_t F1 = KC_F1;
inline basic_t F2 = KC_F2;
inline basic_t F3 = KC_F3;
inline basic_t F4 = KC_F4;
inline basic_t F5 = KC_F5;
inline basic_t F6 = KC_F6;

inline basic_t F7 = KC_F7;              // 0x40
inline basic_t F8 = KC_F8;
inline basic_t F9 = KC_F9;
inline basic_t F10 = KC_F10;
inline basic_t F11 = KC_F11;
inline basic_t F12 = KC_F12;
inline basic_t PSCREEN = KC_PSCREEN;
inline basic_t SCROLLLOCK = KC_SCROLLLOCK;
inline basic_t PAUSE = KC_PAUSE;
inline basic_t INSERT = KC_INSERT;
inline basic_t HOME = KC_HOME;
inline basic_t PGUP = KC_PGUP;
inline basic_t DELETE = KC_DELETE;
inline basic_t END = KC_END;
inline basic_t PGDOWN = KC_PGDOWN;
inline basic_t RIGHT = KC_RIGHT;

inline basic_t LEFT = KC_LEFT;          // 0x50
inline basic_t DOWN = KC_DOWN;
inline basic_t UP = KC_UP;
inline basic_t NUMLOCK = KC_NUMLOCK;
inline basic_t KP_SLASH = KC_KP_SLASH;
inline basic_t KP_ASTERISK = KC_KP_ASTERISK;
inline basic_t KP_MINUS = KC_KP_MINUS;
inline basic_t KP_PLUS = KC_KP_PLUS;
inline basic_t KP_ENTER = KC_KP_ENTER;
inline basic_t KP_1 = KC_KP_1;
inline basic_t KP_2 = KC_KP_2;
inline basic_t KP_3 = KC_KP_3;
inline basic_t KP_4 = KC_KP_4;
inline basic_t KP_5 = KC_KP_5;
inline basic_t KP_6 = KC_KP_6;
inline basic_t KP_7 = KC_KP_7;

inline basic_t KP_8 = KC_KP_8;          // 0x60
inline basic_t KP_9 = KC_KP_9;
inline basic_t KP_0 = KC_KP_0;
inline basic_t KP_DOT = KC_KP_DOT;
inline basic_t NONUS_BSLASH = KC_NONUS_BSLASH;  // Non-US \ and |
inline basic_t APPLICATION = KC_APPLICATION;
inline basic_t POWER = KC_POWER;
inline basic_t KP_EQUAL = KC_KP_EQUAL;
inline basic_t F13 = KC_F13;
inline basic_t F14 = KC_F14;
inline basic_t F15 = KC_F15;
inline basic_t F16 = KC_F16;
inline basic_t F17 = KC_F17;
inline basic_t F18 = KC_F18;
inline basic_t F19 = KC_F19;
inline basic_t F20 = KC_F20;

inline basic_t F21 = KC_F21;            // 0x70
inline basic_t F22 = KC_F22;
inline basic_t F23 = KC_F23;
inline basic_t F24 = KC_F24;
inline basic_t EXECUTE = KC_EXECUTE;
inline basic_t HELP = KC_HELP;
inline basic_t MENU = KC_MENU;
inline basic_t SELECT = KC_SELECT;
inline basic_t STOP = KC_STOP;
inline basic_t AGAIN = KC_AGAIN;
inline basic_t UNDO = KC_UNDO;
inline basic_t CUT = KC_CUT;
inline basic_t COPY = KC_COPY;
inline basic_t PASTE = KC_PASTE;
inline basic_t FIND = KC_FIND;
inline basic_t MUTE = KC_MUTE;

inline basic_t VOLUP = KC_VOLUP;        // 0x80
inline basic_t VOLDOWN = KC_VOLDOWN;
inline basic_t LOCKING_CAPS = KC_LOCKING_CAPS;      // locking Caps Lock
inline basic_t LOCKING_NUM = KC_LOCKING_NUM;        // locking Num Lock
inline basic_t LOCKING_SCROLL = KC_LOCKING_SCROLL;  // locking Scroll Lock
inline basic_t KP_COMMA = KC_KP_COMMA;
inline basic_t KP_EQUAL_AS400 = KC_KP_EQUAL_AS400;  // equal sign on AS/400
inline basic_t INT1 = KC_INT1;
inline basic_t INT2 = KC_INT2;
inline basic_t INT3 = KC_INT3;
inline basic_t INT4 = KC_INT4;
inline basic_t INT5 = KC_INT5;
inline basic_t INT6 = KC_INT6;
inline basic_t INT7 = KC_INT7;
inline basic_t INT8 = KC_INT8;
inline basic_t INT9 = KC_INT9;

inline basic_t LANG1 = KC_LANG1;        // 0x90
inline basic_t LANG2 = KC_LANG2;
inline basic_t LANG3 = KC_LANG3;
inline basic_t LANG4 = KC_LANG4;
inline basic_t LANG5 = KC_LANG5;
inline basic_t LANG6 = KC_LANG6;
inline basic_t LANG7 = KC_LANG7;
inline basic_t LANG8 = KC_LANG8;
inline basic_t LANG9 = KC_LANG9;
inline basic_t ALT_ERASE = KC_ALT_ERASE;
inline basic_t SYSREQ = KC_SYSREQ;
inline basic_t CANCEL = KC_CANCEL;
inline basic_t CLEAR = KC_CLEAR;
inline basic_t PRIOR = KC_PRIOR;
inline basic_t RETURN = KC_RETURN;
inline basic_t SEPARATOR = KC_SEPARATOR;

inline basic_t OUT = KC_OUT;            // 0xA0
inline basic_t OPER = KC_OPER;
inline basic_t CLEAR_AGAIN = KC_CLEAR_AGAIN;
inline basic_t CRSEL = KC_CRSEL;
inline basic_t EXSEL = KC_EXSEL;        // 0xA4

// 0xA5 to 0xAF - RESERVED
inline basic_t RESERVED_A5 = KC_RESERVED_A5;
inline basic_t RESERVED_A6 = KC_RESERVED_A6;
inline basic_t RESERVED_A7 = KC_RESERVED_A7;
inline basic_t RESERVED_A8 = KC_RESERVED_A8;
inline basic_t RESERVED_A9 = KC_RESERVED_A9;
inline basic_t RESERVED_AA = KC_RESERVED_AA;
inline basic_t RESERVED_AB = KC_RESERVED_AB;
inline basic_t RESERVED_AC = KC_RESERVED_AC;
inline basic_t RESERVED_AD = KC_RESERVED_AD;
inline basic_t RESERVED_AE = KC_RESERVED_AE;
inline basic_t RESERVED_AF = KC_RESERVED_AF;

// Modifiers
inline basic_t LCTRL = KC_LCTRL;        // 0xE0
inline basic_t LSHIFT = KC_LSHIFT;
inline basic_t LALT = KC_LALT;
inline basic_t LGUI = KC_LGUI;
inline basic_t RCTRL = KC_RCTRL;
inline basic_t RSHIFT = KC_RSHIFT;
inline basic_t RALT = KC_RALT;
inline basic_t RGUI = KC_RGUI;



// Short names for ease of definition of keymap

inline basic_t& LCTL  = LCTRL;
inline basic_t& RCTL  = RCTRL;
inline basic_t& LSFT  = LSHIFT;
inline basic_t& RSFT  = RSHIFT;
inline basic_t& ESC   = ESCAPE;
inline basic_t& BKSP  = BSPACE;
inline basic_t& ENT   = ENTER;
inline basic_t& DEL   = DELETE;
inline basic_t& INS   = INSERT;
inline basic_t& CAPS  = CAPSLOCK;
inline basic_t& PGDN  = PGDOWN;
inline basic_t& PSCR  = PSCREEN;
inline basic_t& SLCK  = SCROLLLOCK;
inline basic_t& NLCK  = NUMLOCK;
inline basic_t& SPC   = SPACE;
inline basic_t& EQL   = EQUAL;
inline basic_t& GRV   = GRAVE;
inline basic_t& RBRKT = RBRACKET;
inline basic_t& LBRKT = LBRACKET;
inline basic_t& COLON = SCOLON;
inline basic_t& APP   = APPLICATION;
inline basic_t& NUHS  = NONUS_HASH;
inline basic_t& NUBS  = NONUS_BSLASH;
inline basic_t& LCAP  = LOCKING_CAPS;
inline basic_t& LNUM  = LOCKING_NUM;
inline basic_t& LSCR  = LOCKING_SCROLL;
inline basic_t& ERASE = ALT_ERASE;
inline basic_t& CLR   = CLEAR;
// Japanese specific
inline basic_t& ZKHK  = GRAVE;
inline basic_t& RO    = INT1;
inline basic_t& KANA  = INT2;
inline basic_t& JYEN  = INT3;
inline basic_t& HENK  = INT4;
inline basic_t& MHEN  = INT5;
// Korean specific
inline basic_t& HAEN  = LANG1;
inline basic_t& HANJ  = LANG2;
// Keypad
inline basic_t& P1    = KP_1;
inline basic_t& P2    = KP_2;
inline basic_t& P3    = KP_3;
inline basic_t& P4    = KP_4;
inline basic_t& P5    = KP_5;
inline basic_t& P6    = KP_6;
inline basic_t& P7    = KP_7;
inline basic_t& P8    = KP_8;
inline basic_t& P9    = KP_9;
inline basic_t& P0    = KP_0;
inline basic_t& PDOT  = KP_DOT;
inline basic_t& PCMM  = KP_COMMA;
inline basic_t& PSLS  = KP_SLASH;
inline basic_t& PAST  = KP_ASTERISK;
inline basic_t& PMNS  = KP_MINUS;
inline basic_t& PPLS  = KP_PLUS;
inline basic_t& PEQL  = KP_EQUAL;
inline basic_t& PENT  = KP_ENTER;
// Unix function key
inline basic_t& EXEC  = EXECUTE;
inline basic_t& SLCT  = SELECT;
inline basic_t& AGIN  = AGAIN;
inline basic_t& PSTE  = PASTE;
// Mousekey
// inline basic_t& MS_U = MS_UP;
// inline basic_t& MS_D = MS_DOWN;
// inline basic_t& MS_L = MS_LEFT;
// inline basic_t& MS_R = MS_RIGHT;
// inline basic_t& BTN1 = MS_BTN1;
// inline basic_t& BTN2 = MS_BTN2;
// inline basic_t& BTN3 = MS_BTN3;
// inline basic_t& BTN4 = MS_BTN4;
// inline basic_t& BTN5 = MS_BTN5;
// inline basic_t& WH_U = MS_WH_UP;
// inline basic_t& WH_D = MS_WH_DOWN;
// inline basic_t& WH_L = MS_WH_LEFT;
// inline basic_t& WH_R = MS_WH_RIGHT;
// inline basic_t& ACL0 = MS_ACCEL0;
// inline basic_t& ACL1 = MS_ACCEL1;
// inline basic_t& ACL2 = MS_ACCEL2;

// GUI key aliases
inline basic_t& LCMD  = LGUI;
inline basic_t& LWIN  = LGUI;
inline basic_t& RCMD  = RGUI;
inline basic_t& RWIN  = RGUI;

}  // namespace key
