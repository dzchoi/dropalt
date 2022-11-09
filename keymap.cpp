#include "literal.hpp"
#include "map.hpp"
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
tap_hold_t xLCTL { ESC, LCTL };
tap_hold_fast_t xSPC { SPC, RSFT };
// More ideas:
//  - SPC + BKSP == DEL
//  - Left + Left == Ctrl + Left
//  - Left + Left + Left == Ctrl + Left + Left



class test_t: public map_t, public timer_t {
public:
    test_t(): timer_t(500 *US_PER_MS) {}

    void on_press(pmap_t* slot) {
        start_timer(slot);
        start_defer_presses();
    }

    void on_release(pmap_t*) {
        stop_timer();
        stop_defer_presses();

        if ( FN.is_pressing() && xLCTL.is_pressing() )
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
            if ( xLCTL.is_pressing() )
                // assert( false );
                WDT->CLEAR.reg = 0;  // anything other than 0xA5
            else
                perform_usbhub_switchover();
        }
    }
} test;



class ext_lsft_t: public map_t {
public:
    void on_press(pmap_t*) {
        m_code = xSPC.is_pressing() ? KC_SPACE : KC_LSHIFT;
        send_press(m_code);
    }

    void on_release(pmap_t*) {
        send_release(m_code);
        m_code = 0;
    }

private:
    uint8_t m_code = 0;
} xLSFT;



// Todo: Allocate them in PROGMEM(?).
pmap_t maps[MATRIX_ROWS][MATRIX_COLS] = {
    GRV, _1, _2, _3, _4, _5, _6, _7, _8, _9, _0, MINUS, EQL, BKSP, DEL,

    TAB, Q, W, E, R, T, Y, U, I, O, P, LBRKT, RBRKT, BSLASH, HOME,

    xLCTL, A, S, D, F, G, H, J, K, L, COLON, QUOTE, ___, ENT, PGUP,

    xLSFT, ___, Z, X, C, V, B, N, M, COMMA, DOT, SLASH, RSFT, UP, PGDN,

    FN, LGUI, LALT, ___, ___, ___, xSPC, ___, ___, ___,
        RCTL, test /*RALT*/, LEFT, DOWN, RIGHT,
};

}  // namespace key
