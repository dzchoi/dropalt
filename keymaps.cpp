#define ENABLE_DEBUG 1          // Will also affect DEBUG()'s in included files
#include "debug.h"

#include "features.hpp"         // for TAPPING_TERM_MS
#include "if.hpp"
#include "literal.hpp"
#include "map.hpp"
#include "norepeat.hpp"
#include "persistent.hpp"       // for persistent::obj()
#include "tap_dance.hpp"
#include "tap_hold.hpp"
#include "timer.hpp"
#include "usb_thread.hpp"       // for hid_keyboard.get_led_state()



namespace key {

/*
// Default keymap for reference
uint8_t keymap[MATRIX_ROWS][MATRIX_COLS] = {
    KC_ESC,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_BSPC, KC_DEL,  \

    KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS, KC_HOME, \

    KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT, KC_NO,   KC_ENT,  KC_PGUP, \

    KC_LSFT, KC_NO,   KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH, KC_RSFT, KC_UP,   KC_PGDN, \

    KC_LCTL, KC_LGUI, KC_LALT, KC_NO,   KC_NO,   KC_NO,   KC_SPC,  KC_NO,   KC_NO,   KC_NO,   KC_RALT, KC_TRNS, KC_LEFT, KC_DOWN, KC_RGHT  \
};
*/

// Custom modifiers
map_t FN;
map_t FN2;

static bool FN_pressed() { return FN.is_pressed(); }
static bool FN2_pressed() { return FN2.is_pressed(); }

// Hold Tab = FN2
tap_hold_t t_TAB { TAB, FN2, HOLD_PREFERRED };

// Tap LCTL = ESC
tap_hold_t t_LCTL { ESC, LCTL, HOLD_PREFERRED };  // sizeof = 72

// Hold ENTER = FN
tap_hold_t t_ENT { ENT, FN, HOLD_PREFERRED };

// Hold SPC = RSFT
tap_hold_t t_SPC { SPC, RSFT, BALANCED };  // sizeof = 72

// FN + P = PrtScr
if_t m_P { FN_pressed, PSCR, P };  // sizeof = 24

// FN + LBRKT = Break/Pause
if_t m_LBRKT { FN_pressed, PAUSE, LBRKT };

// FN + RBRKT = ScrollLock
if_t m_RBRKT { FN_pressed, SCROLLLOCK, RBRKT };

// FN + H/J/K/L = Arrow keys, FN2 + H/J/K/L = Home/PgDn/PgUp/End
if_t m_H { FN_pressed, LEFT, if_t(FN2_pressed, HOME, H) };  // sizeof = 44
if_t m_J { FN_pressed, DOWN, if_t(FN2_pressed, PGDN, J) };
if_t m_K { FN_pressed, UP, if_t(FN2_pressed, PGUP, K) };
if_t m_L { FN_pressed, RIGHT, if_t(FN2_pressed, END, L) };

// FN + BkSp = DEL
if_t m_BKSP { FN_pressed, DEL, BKSP };

// Alternatively, m_BKSP defines RSFT as its modifier. With t_SPC instead, when m_BKSP is
// pressed within the tapping term of t_SPC the tap version of t_SPC (i.e. SPC) would be
// sent immediately after sending DEL.
// mod_morph_t<UNDO_MOD> m_BKSP { BKSP, DEL, RSFT };

// FN + ` = Power
if_t m_GRV { FN_pressed, POWER, GRV };

// t_SPC + LSFT = SPC, Double LSFT = CapsLock, LSFT (when CapsLock on) = CapsLock
if_t t_LSFT {  // sizeof = 112
    [](){ return t_SPC.is_pressed(); },
    SPC,
    if_t (
        [](){ return (usb_thread::obj().hid_keyboard.get_led_state() & 0x2) != 0; },
        CAPSLOCK,
        tap_dance_double_t(LSFT, CAPSLOCK)
    )
};

// FN + 1 = F1 or Hold 1 = F1
if_t m_1 { FN_pressed, F1, tap_hold_t(_1, norepeat_t(F1)) };  // sizeof = 100
if_t m_2 { FN_pressed, F2, tap_hold_t(_2, norepeat_t(F2)) };
if_t m_3 { FN_pressed, F3, tap_hold_t(_3, norepeat_t(F3)) };
if_t m_4 { FN_pressed, F4, tap_hold_t(_4, norepeat_t(F4)) };
if_t m_5 { FN_pressed, F5, tap_hold_t(_5, norepeat_t(F5)) };
if_t m_6 { FN_pressed, F6, tap_hold_t(_6, norepeat_t(F6)) };
if_t m_7 { FN_pressed, F7, tap_hold_t(_7, norepeat_t(F7)) };
if_t m_8 { FN_pressed, F8, tap_hold_t(_8, norepeat_t(F8)) };
if_t m_9 { FN_pressed, F9, tap_hold_t(_9, norepeat_t(F9)) };
if_t m_0 { FN_pressed, F10, tap_hold_t(_0, norepeat_t(F10)) };
if_t m_MINUS { FN_pressed, F11, tap_hold_t(MINUS, norepeat_t(F11)) };
if_t m_EQL { FN_pressed, F12, tap_hold_t(EQL, norepeat_t(F12)) };



// It presses the modifier before the second tap.
class tap_dance_morph_t: public map_dance_t {
public:
    constexpr tap_dance_morph_t(
        map_t& original, map_t& modifier, uint32_t tapping_term_ms =TAPPING_TERM_MS)
    : map_dance_t(tapping_term_ms), m_original(original), m_modifier(modifier) {}

private:
    void on_press(pmap_t* slot) {
        if ( get_step() > 1 ) {
            m_original.release(slot);
            if ( get_step() == 2 )
                m_modifier.press(slot);
        }
        m_original.press(slot);
    }

    void on_release(pmap_t* slot) {
        m_original.release(slot);
        if ( get_step() > 1 )
            m_modifier.release(slot);
    }

    map_t& m_original;
    map_t& m_modifier;
};

// tap_dance_morph_t t_LEFT { LEFT, RCTRL };
// tap_dance_morph_t t_RIGHT { RIGHT, RCTRL };

// More keymap ideas:
//  - qq to send TAB
//  - ;; to send ::



class test_t: public map_t, public timer_t {  // sizeof = 48
public:
    constexpr test_t(): timer_t(500) {}

private:
    void on_press(pmap_t* slot) {
        start_timer(slot);
        // start_defer_presses();
    }

    void on_release(pmap_t*) {
        stop_timer();
        // stop_defer_presses();

        if ( FN.is_pressed() ) {
            if ( t_LCTL.is_pressed() )
                system_reset();
            else {
                const uint16_t color = persistent::obj().led_color.h == ORANGE
                    ? SPRING_GREEN : ORANGE;
                persistent::obj().write(&persistent::led_color, hsv_t{ color, 255, 255 });
            }
        }

        // if ( t_LCTL.is_pressed() ) {
        //     if ( LSFT.is_pressed() )
        //         set_extra_usbport_back_to_automatic();
        //     else
        //         enable_extra_usbport_manually();
        // }
    }

    void on_timeout(pmap_t*) {
        if ( FN.is_pressed() ) {
            if ( t_LCTL.is_pressed() )
                // assert( false );
                WDT->CLEAR.reg = 0;  // anything other than 0xA5
            else
                perform_usbport_switchover();
        }
    }
} TEST;



pmap_t maps[MATRIX_ROWS][MATRIX_COLS] = {
    m_GRV, m_1, m_2, m_3, m_4, m_5, m_6, m_7, m_8, m_9, m_0, m_MINUS, m_EQL, m_BKSP, HOME,

    t_TAB, Q, W, E, R, T, Y, U, I, O, m_P, m_LBRKT, m_RBRKT, BSLASH, END,

    t_LCTL, A, S, D, F, G, m_H, m_J, m_K, m_L, COLON, QUOTE, ___, t_ENT, PGUP,

    t_LSFT, ___, Z, X, C, V, B, N, M, COMMA, DOT, SLASH, RSFT, UP, PGDN,

    LALT, LGUI, FN, ___, ___, ___, t_SPC, ___, ___, ___,
        RCTL, TEST /*RALT*/, LEFT, DOWN, RIGHT,
};

}  // namespace key
