#pragma once

#include "hid_keycodes.hpp"
#include "keymap.hpp"



namespace key {

class literal_t: public map_t {
public:
    constexpr literal_t(uint8_t keycode): m_code(keycode) {}

    void on_press(pmap_t*) { m_pressing = true; send_press(m_code); }

    void on_release(pmap_t*) { m_pressing = false; send_release(m_code); }

    bool is_pressing() const { return m_pressing; }

    uint8_t keycode() const { return m_code; }

private:
    const uint8_t m_code;
    bool m_pressing = false;
};



// Note that unused ones will not be allocated thanks to g++.

inline literal_t ROLL_OVER = KC_ROLL_OVER;
inline literal_t POST_FAIL = KC_POST_FAIL;
inline literal_t UNDEFINED = KC_UNDEFINED;
inline literal_t A = KC_A;
inline literal_t B = KC_B;
inline literal_t C = KC_C;
inline literal_t D = KC_D;
inline literal_t E = KC_E;
inline literal_t F = KC_F;
inline literal_t G = KC_G;
inline literal_t H = KC_H;
inline literal_t I = KC_I;
inline literal_t J = KC_J;
inline literal_t K = KC_K;
inline literal_t L = KC_L;

inline literal_t M = KC_M;              // 0x10
inline literal_t N = KC_N;
inline literal_t O = KC_O;
inline literal_t P = KC_P;
inline literal_t Q = KC_Q;
inline literal_t R = KC_R;
inline literal_t S = KC_S;
inline literal_t T = KC_T;
inline literal_t U = KC_U;
inline literal_t V = KC_V;
inline literal_t W = KC_W;
inline literal_t X = KC_X;
inline literal_t Y = KC_Y;
inline literal_t Z = KC_Z;
inline literal_t _1 = KC_1;
inline literal_t _2 = KC_2;

inline literal_t _3 = KC_3;             // 0x20
inline literal_t _4 = KC_4;
inline literal_t _5 = KC_5;
inline literal_t _6 = KC_6;
inline literal_t _7 = KC_7;
inline literal_t _8 = KC_8;
inline literal_t _9 = KC_9;
inline literal_t _0 = KC_0;
inline literal_t ENTER = KC_ENTER;
inline literal_t ESCAPE = KC_ESCAPE;
inline literal_t BSPACE = KC_BSPACE;
inline literal_t TAB = KC_TAB;
inline literal_t SPACE = KC_SPACE;
inline literal_t MINUS = KC_MINUS;
inline literal_t EQUAL = KC_EQUAL;
inline literal_t LBRACKET = KC_LBRACKET;

inline literal_t RBRACKET = KC_RBRACKET;  // 0x30
inline literal_t BSLASH = KC_BSLASH;          // \ (and |)
inline literal_t NONUS_HASH = KC_NONUS_HASH;  // Non-US # and ~
inline literal_t SCOLON = KC_SCOLON;          // ; (and :)
inline literal_t QUOTE = KC_QUOTE;            // ' and "
inline literal_t GRAVE = KC_GRAVE;            // Grave accent and tilde
inline literal_t COMMA = KC_COMMA;            // , and <
inline literal_t DOT = KC_DOT;                // . and >
inline literal_t SLASH = KC_SLASH;            // / and ?
inline literal_t CAPSLOCK = KC_CAPSLOCK;
inline literal_t F1 = KC_F1;
inline literal_t F2 = KC_F2;
inline literal_t F3 = KC_F3;
inline literal_t F4 = KC_F4;
inline literal_t F5 = KC_F5;
inline literal_t F6 = KC_F6;

inline literal_t F7 = KC_F7;            // 0x40
inline literal_t F8 = KC_F8;
inline literal_t F9 = KC_F9;
inline literal_t F10 = KC_F10;
inline literal_t F11 = KC_F11;
inline literal_t F12 = KC_F12;
inline literal_t PSCREEN = KC_PSCREEN;
inline literal_t SCROLLLOCK = KC_SCROLLLOCK;
inline literal_t PAUSE = KC_PAUSE;
inline literal_t INSERT = KC_INSERT;
inline literal_t HOME = KC_HOME;
inline literal_t PGUP = KC_PGUP;
inline literal_t DELETE = KC_DELETE;
inline literal_t END = KC_END;
inline literal_t PGDOWN = KC_PGDOWN;
inline literal_t RIGHT = KC_RIGHT;

inline literal_t LEFT = KC_LEFT;        // 0x50
inline literal_t DOWN = KC_DOWN;
inline literal_t UP = KC_UP;
inline literal_t NUMLOCK = KC_NUMLOCK;
inline literal_t KP_SLASH = KC_KP_SLASH;
inline literal_t KP_ASTERISK = KC_KP_ASTERISK;
inline literal_t KP_MINUS = KC_KP_MINUS;
inline literal_t KP_PLUS = KC_KP_PLUS;
inline literal_t KP_ENTER = KC_KP_ENTER;
inline literal_t KP_1 = KC_KP_1;
inline literal_t KP_2 = KC_KP_2;
inline literal_t KP_3 = KC_KP_3;
inline literal_t KP_4 = KC_KP_4;
inline literal_t KP_5 = KC_KP_5;
inline literal_t KP_6 = KC_KP_6;
inline literal_t KP_7 = KC_KP_7;

inline literal_t KP_8 = KC_KP_8;        // 0x60
inline literal_t KP_9 = KC_KP_9;
inline literal_t KP_0 = KC_KP_0;
inline literal_t KP_DOT = KC_KP_DOT;
inline literal_t NONUS_BSLASH = KC_NONUS_BSLASH;  // Non-US \ and |
inline literal_t APPLICATION = KC_APPLICATION;
inline literal_t POWER = KC_POWER;
inline literal_t KP_EQUAL = KC_KP_EQUAL;
inline literal_t F13 = KC_F13;
inline literal_t F14 = KC_F14;
inline literal_t F15 = KC_F15;
inline literal_t F16 = KC_F16;
inline literal_t F17 = KC_F17;
inline literal_t F18 = KC_F18;
inline literal_t F19 = KC_F19;
inline literal_t F20 = KC_F20;

inline literal_t F21 = KC_F21;          // 0x70
inline literal_t F22 = KC_F22;
inline literal_t F23 = KC_F23;
inline literal_t F24 = KC_F24;
inline literal_t EXECUTE = KC_EXECUTE;
inline literal_t HELP = KC_HELP;
inline literal_t MENU = KC_MENU;
inline literal_t SELECT = KC_SELECT;
inline literal_t STOP = KC_STOP;
inline literal_t AGAIN = KC_AGAIN;
inline literal_t UNDO = KC_UNDO;
inline literal_t CUT = KC_CUT;
inline literal_t COPY = KC_COPY;
inline literal_t PASTE = KC_PASTE;
inline literal_t FIND = KC_FIND;
inline literal_t MUTE = KC_MUTE;

inline literal_t VOLUP = KC_VOLUP;      // 0x80
inline literal_t VOLDOWN = KC_VOLDOWN;
inline literal_t LOCKING_CAPS = KC_LOCKING_CAPS;      // locking Caps Lock
inline literal_t LOCKING_NUM = KC_LOCKING_NUM;        // locking Num Lock
inline literal_t LOCKING_SCROLL = KC_LOCKING_SCROLL;  // locking Scroll Lock
inline literal_t KP_COMMA = KC_KP_COMMA;
inline literal_t KP_EQUAL_AS400 = KC_KP_EQUAL_AS400;  // equal sign on AS/400
inline literal_t INT1 = KC_INT1;
inline literal_t INT2 = KC_INT2;
inline literal_t INT3 = KC_INT3;
inline literal_t INT4 = KC_INT4;
inline literal_t INT5 = KC_INT5;
inline literal_t INT6 = KC_INT6;
inline literal_t INT7 = KC_INT7;
inline literal_t INT8 = KC_INT8;
inline literal_t INT9 = KC_INT9;

inline literal_t LANG1 = KC_LANG1;      // 0x90
inline literal_t LANG2 = KC_LANG2;
inline literal_t LANG3 = KC_LANG3;
inline literal_t LANG4 = KC_LANG4;
inline literal_t LANG5 = KC_LANG5;
inline literal_t LANG6 = KC_LANG6;
inline literal_t LANG7 = KC_LANG7;
inline literal_t LANG8 = KC_LANG8;
inline literal_t LANG9 = KC_LANG9;
inline literal_t ALT_ERASE = KC_ALT_ERASE;
inline literal_t SYSREQ = KC_SYSREQ;
inline literal_t CANCEL = KC_CANCEL;
inline literal_t CLEAR = KC_CLEAR;
inline literal_t PRIOR = KC_PRIOR;
inline literal_t RETURN = KC_RETURN;
inline literal_t SEPARATOR = KC_SEPARATOR;

inline literal_t OUT = KC_OUT;          // 0xA0
inline literal_t OPER = KC_OPER;
inline literal_t CLEAR_AGAIN = KC_CLEAR_AGAIN;
inline literal_t CRSEL = KC_CRSEL;
inline literal_t EXSEL = KC_EXSEL;      // 0xA4

// 0xA5 to 0xAF - RESERVED
inline literal_t RESERVED_A5 = KC_RESERVED_A5;
inline literal_t RESERVED_A6 = KC_RESERVED_A6;
inline literal_t RESERVED_A7 = KC_RESERVED_A7;
inline literal_t RESERVED_A8 = KC_RESERVED_A8;
inline literal_t RESERVED_A9 = KC_RESERVED_A9;
inline literal_t RESERVED_AA = KC_RESERVED_AA;
inline literal_t RESERVED_AB = KC_RESERVED_AB;
inline literal_t RESERVED_AC = KC_RESERVED_AC;
inline literal_t RESERVED_AD = KC_RESERVED_AD;
inline literal_t RESERVED_AE = KC_RESERVED_AE;
inline literal_t RESERVED_AF = KC_RESERVED_AF;

// Modifiers
inline literal_t LCTRL  = KC_LCTRL;     // 0xE0
inline literal_t LSHIFT = KC_LSHIFT;
inline literal_t LALT   = KC_LALT;
inline literal_t LGUI   = KC_LGUI;
inline literal_t RCTRL  = KC_RCTRL;
inline literal_t RSHIFT = KC_RSHIFT;
inline literal_t RALT   = KC_RALT;
inline literal_t RGUI   = KC_RGUI;



// Short names for ease of definitions for some keymaps

inline literal_t& LCTL  = LCTRL;
inline literal_t& RCTL  = RCTRL;
inline literal_t& LSFT  = LSHIFT;
inline literal_t& RSFT  = RSHIFT;
inline literal_t& ESC   = ESCAPE;
inline literal_t& BKSP  = BSPACE;
inline literal_t& ENT   = ENTER;
inline literal_t& DEL   = DELETE;
inline literal_t& INS   = INSERT;
inline literal_t& CAPS  = CAPSLOCK;
inline literal_t& PGDN  = PGDOWN;
inline literal_t& PSCR  = PSCREEN;
inline literal_t& SLCK  = SCROLLLOCK;
inline literal_t& NLCK  = NUMLOCK;
inline literal_t& SPC   = SPACE;
inline literal_t& EQL   = EQUAL;
inline literal_t& GRV   = GRAVE;
inline literal_t& RBRKT = RBRACKET;
inline literal_t& LBRKT = LBRACKET;
inline literal_t& COLON = SCOLON;
inline literal_t& APP   = APPLICATION;
inline literal_t& NUHS  = NONUS_HASH;
inline literal_t& NUBS  = NONUS_BSLASH;
inline literal_t& LCAP  = LOCKING_CAPS;
inline literal_t& LNUM  = LOCKING_NUM;
inline literal_t& LSCR  = LOCKING_SCROLL;
inline literal_t& ERASE = ALT_ERASE;
inline literal_t& CLR   = CLEAR;
// Japanese specific
inline literal_t& ZKHK  = GRAVE;
inline literal_t& RO    = INT1;
inline literal_t& KANA  = INT2;
inline literal_t& JYEN  = INT3;
inline literal_t& HENK  = INT4;
inline literal_t& MHEN  = INT5;
// Korean specific
inline literal_t& HAEN  = LANG1;
inline literal_t& HANJ  = LANG2;
// Keypad
inline literal_t& P1    = KP_1;
inline literal_t& P2    = KP_2;
inline literal_t& P3    = KP_3;
inline literal_t& P4    = KP_4;
inline literal_t& P5    = KP_5;
inline literal_t& P6    = KP_6;
inline literal_t& P7    = KP_7;
inline literal_t& P8    = KP_8;
inline literal_t& P9    = KP_9;
inline literal_t& P0    = KP_0;
inline literal_t& PDOT  = KP_DOT;
inline literal_t& PCMM  = KP_COMMA;
inline literal_t& PSLS  = KP_SLASH;
inline literal_t& PAST  = KP_ASTERISK;
inline literal_t& PMNS  = KP_MINUS;
inline literal_t& PPLS  = KP_PLUS;
inline literal_t& PEQL  = KP_EQUAL;
inline literal_t& PENT  = KP_ENTER;
// Unix function key
inline literal_t& EXEC  = EXECUTE;
inline literal_t& SLCT  = SELECT;
inline literal_t& AGIN  = AGAIN;
inline literal_t& PSTE  = PASTE;
// Mousekey
// inline literal_t& MS_U = MS_UP;
// inline literal_t& MS_D = MS_DOWN;
// inline literal_t& MS_L = MS_LEFT;
// inline literal_t& MS_R = MS_RIGHT;
// inline literal_t& BTN1 = MS_BTN1;
// inline literal_t& BTN2 = MS_BTN2;
// inline literal_t& BTN3 = MS_BTN3;
// inline literal_t& BTN4 = MS_BTN4;
// inline literal_t& BTN5 = MS_BTN5;
// inline literal_t& WH_U = MS_WH_UP;
// inline literal_t& WH_D = MS_WH_DOWN;
// inline literal_t& WH_L = MS_WH_LEFT;
// inline literal_t& WH_R = MS_WH_RIGHT;
// inline literal_t& ACL0 = MS_ACCEL0;
// inline literal_t& ACL1 = MS_ACCEL1;
// inline literal_t& ACL2 = MS_ACCEL2;

// GUI key aliases
inline literal_t& LCMD = LGUI;
inline literal_t& LWIN = LGUI;
inline literal_t& RCMD = RGUI;
inline literal_t& RWIN = RGUI;

}  // namespace key
