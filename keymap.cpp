#include "literal.hpp"
#include "map.hpp"
#include "modified.hpp"
#include "tap_hold.hpp"
#include "timer.hpp"



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

map_t FN;
tap_hold_t tLCTL { ESC, LCTL };
tap_hold_fast_t tSPC { SPC, RSFT };
modified_t mLSFT { LSFT, SPC, tSPC };

// tap_hold_t t1 { _1, F1 };
modified_t mt1 { _1, F1, FN };  // consumes 100 bytes per each mt?
// tap_hold_t t2 { _2, F2 };
modified_t mt2 { _2, F2, FN };
// tap_hold_t t3 { _3, F3 };
modified_t mt3 { _3, F3, FN };
// tap_hold_t t4 { _4, F4 };
modified_t mt4 { _4, F4, FN };
// tap_hold_t t5 { _5, F5 };
modified_t mt5 { _5, F5, FN };
// tap_hold_t t6 { _6, F6 };
modified_t mt6 { _6, F6, FN };
// tap_hold_t t7 { _7, F7 };
modified_t mt7 { _7, F7, FN };
// tap_hold_t t8 { _8, F8 };
modified_t mt8 { _8, F8, FN };
// tap_hold_t t9 { _9, F9 };
modified_t mt9 { _9, F9, FN };
// tap_hold_t t0 { _0, F10 };
modified_t mt0 { _0, F10, FN };

// More ideas:
//  - SPC + BKSP == DEL
//  - Left + Left == Ctrl + Left
//  - Left + Left + Left == Ctrl + Left + Left



class test_t: public map_t, public timer_t {
public:
    constexpr test_t(): timer_t(500 *US_PER_MS) {}

private:
    void on_press(pmap_t* slot) {
        start_timer(slot);
        start_defer_presses();
    }

    void on_release(pmap_t*) {
        stop_timer();
        stop_defer_presses();

        if ( FN.is_pressing() && tLCTL.is_pressing() )
            system_reset();

        // Test key sequence
        // send_press(KC_SCOLON);
        // send_press(KC_F);
        // send_press(KC_A);
        // send_release(KC_SCOLON);
        // send_release(KC_F);
        // send_release(KC_A);
    }

    void on_timeout(pmap_t*) {
        if ( FN.is_pressing() ) {
            if ( tLCTL.is_pressing() )
                // assert( false );
                WDT->CLEAR.reg = 0;  // anything other than 0xA5
            else
                perform_usbhub_switchover();
        }
    }
} test;



// Todo: Allocate them in PROGMEM(?).
pmap_t maps[MATRIX_ROWS][MATRIX_COLS] = {
    GRV, mt1, mt2, mt3, mt4, mt5, mt6, mt7, mt8, mt9, mt0, MINUS, EQL, BKSP, DEL,

    TAB, Q, W, E, R, T, Y, U, I, O, P, LBRKT, RBRKT, BSLASH, HOME,

    tLCTL, A, S, D, F, G, H, J, K, L, COLON, QUOTE, ___, ENT, PGUP,

    mLSFT, ___, Z, X, C, V, B, N, M, COMMA, DOT, SLASH, RSFT, UP, PGDN,

    FN, LGUI, LALT, ___, ___, ___, tSPC, ___, ___, ___,
        RCTL, test /*RALT*/, LEFT, DOWN, RIGHT,
};

}  // namespace key
