#define ENABLE_DEBUG 1          // Will also affect DEBUG()'s in included files
#include "debug.h"

#include "features.hpp"         // for TAPPING_TERM_MS
#include "literal.hpp"
#include "map.hpp"
#include "mod_morph.hpp"
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

// Hold Tab = FN2
tap_hold_t t_TAB { TAB, FN2, HOLD_PREFERRED };

// Tap LCTL = ESC
tap_hold_t t_LCTL { ESC, LCTL, HOLD_PREFERRED };

// Hold ENTER = FN
tap_hold_t t_ENT { ENT, FN, HOLD_PREFERRED };

// Hold SPC = RSFT
tap_hold_t t_SPC { SPC, RSFT, BALANCED };

// FN + P = PrtScr
mod_morph_t m_P { P, PSCR, FN };

// FN + LBRKT = Break/Pause
mod_morph_t m_LBRKT { LBRKT, PAUSE, FN };

// FN + RBRKT = ScrollLock
mod_morph_t m_RBRKT { RBRKT, SCROLLLOCK, FN };

// FN + H/J/K/L = Arrow keys
mod_morph_t m_H { H, LEFT, FN };
mod_morph_t m_J { J, DOWN, FN };
mod_morph_t m_K { K, UP, FN };
mod_morph_t m_L { L, RIGHT, FN };

// Todo: Tab as second custom modifier.
//  - FN + FN2 + H = Home
//  - FN + FN2 + L = End
//  - FN + FN2 + K = PgUp
//  - FN + FN2 + J = PgDn

// FN + BkSp = DEL
mod_morph_t m_BKSP { BKSP, DEL, FN };

// Alternatively, m_BKSP defines RSFT as its modifier. With t_SPC instead, when m_BKSP is
// pressed within the tapping term of t_SPC the tap version of t_SPC (i.e. SPC) would be
// sent immediately after sending DEL.
// mod_morph_t<UNDO_MOD> m_BKSP { BKSP, DEL, RSFT };

// FN + ` = Power
mod_morph_t m_GRV { GRV, POWER, FN };



template <class K>
class tap_capslock_t: public map_dance_t {
    // Todo: Implement map_conditional(cond, key_if_true, key_otherwise), so that t_LSFT
    //  can be defined as:
    //  mod_morph_t t_LSFT {
    //      map_conditional {
    //          [](){ return hid_keyboard.get_led_state() & 0x2; },
    //          CAPSLOCK,
    //          tap_dance_double_t { LSFT, CAPSLOCK }
    //      },
    //      SPC,
    //      t_SPC
    //  };

public:
    constexpr tap_capslock_t(K&& once, uint32_t tapping_term_ms =TAPPING_TERM_MS)
    : map_dance_t(tapping_term_ms), m_once(std::forward<K>(once)) {}

private:
    void on_press(pmap_t* slot) {
        if ( get_step() == 1 ) {
            if ( usb_thread::obj().hid_keyboard.get_led_state() & 0x2 ) {
                m_ponce = &m_twice;
                finish();
            }
            else
                m_ponce = &m_once;

            m_ponce->press(slot);
        }

        else {
            m_once.release(slot);
            m_twice.press(slot);
            finish();
        }
    }

    void on_release(pmap_t* slot) {
        if ( get_step() == 1 )
            m_ponce->release(slot);
        else
            m_twice.release(slot);
    }

    K m_once;
    map_t* m_ponce = nullptr;
    map_t& m_twice = CAPSLOCK;
};

template <class T>
tap_capslock_t(T&&) -> tap_capslock_t<obj_or_ref_t<T>>;

template <class T>
tap_capslock_t(T&&, uint32_t) -> tap_capslock_t<obj_or_ref_t<T>>;

// t_SPC + LSFT = SPC
// mod_morph_t m_LSFT { LSFT, SPC, t_SPC };

tap_capslock_t t_LSFT { mod_morph_t { LSFT, SPC, t_SPC } };



mod_morph_t m_1 { _1, F1, FN };  // sizeof = 24
mod_morph_t m_2 { _2, F2, FN };
mod_morph_t m_3 { _3, F3, FN };
mod_morph_t m_4 { _4, F4, FN };
mod_morph_t m_5 { _5, F5, FN };
mod_morph_t m_6 { _6, F6, FN };
mod_morph_t m_7 { _7, F7, FN };
mod_morph_t m_8 { _8, F8, FN };
// Todo: Block repeated press of Function keys for tap_hold_t.
// mod_morph_t m_9 { tap_hold_t { _9, F9 }, F9, FN };
mod_morph_t m_9 { _9, F9, FN };
// mod_morph_t m_0 { tap_hold_t { _0, F10 }, F10, FN };
mod_morph_t m_0 { _0, F10, FN };

// tap_hold_t t_MINUS { MINUS, F11 };
mod_morph_t m_MINUS { MINUS, F11, FN };
// tap_hold_t t_EQL { EQL, F12 };
mod_morph_t m_EQL { EQL, F12, FN };



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
//  - double shift to send CapsLock
//  - ;; to send ::



class test_t: public map_t, public timer_t {
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

        if ( FN.is_pressing() ) {
            if ( t_LCTL.is_pressing() )
                system_reset();
            // else if ( LSFT.is_pressing() )
            //     set_extra_usbport_back_to_automatic();
            // else
            //     enable_extra_usbport_manually();
        }
        else {
            const uint16_t color = persistent::obj().led_color.h == ORANGE
                ? SPRING_GREEN : ORANGE;
            persistent::obj().write(&persistent::led_color, hsv_t{ color, 255, 255 });
        }

        // DEBUG("test: map_t=%d literal_t=%d timer_t=%d tap_hold_t=%d mod_morph_t=%d\n",
        //     sizeof(map_t), sizeof(literal_t), sizeof(timer_t),
        //     sizeof(tap_hold_t<HOLD_PREFERRED>), sizeof(mod_morph_t<UNDO_MOD>));
    }

    void on_timeout(pmap_t*) {
        if ( FN.is_pressing() ) {
            if ( t_LCTL.is_pressing() )
                // assert( false );
                WDT->CLEAR.reg = 0;  // anything other than 0xA5
            else
                perform_usbport_switchover();
        }
    }
} test;



pmap_t maps[MATRIX_ROWS][MATRIX_COLS] = {
    m_GRV, m_1, m_2, m_3, m_4, m_5, m_6, m_7, m_8, m_9, m_0, m_MINUS, m_EQL, m_BKSP, HOME,

    TAB, Q, W, E, R, T, Y, U, I, O, m_P, m_LBRKT, m_RBRKT, BSLASH, END,

    t_LCTL, A, S, D, F, G, m_H, m_J, m_K, m_L, COLON, QUOTE, ___, t_ENT, PGUP,

    t_LSFT, ___, Z, X, C, V, B, N, M, COMMA, DOT, SLASH, RSFT, UP, PGDN,

    LALT, LGUI, FN, ___, ___, ___, t_SPC, ___, ___, ___,
        RCTL, test /*RALT*/, LEFT, DOWN, RIGHT,
};

}  // namespace key
