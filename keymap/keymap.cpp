#include "literal.hpp"
#include "tap_hold.hpp"



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

tap_hold_t ext_LCTL { ESC, LCTL };
tap_hold_fast_t ext_SPC { SPC, RSFT };
// More ideas:
//  - SPC + BKSP == DEL
//  - Left + Left == Ctrl + Left



class pressing_t: public map_t {
public:
    void on_press(pmap_t*) { m_pressing = true; }
    void on_release(pmap_t*) { m_pressing = false; }
    bool is_pressing() const { return m_pressing; }

private:
    bool m_pressing = false;
};

inline pressing_t FN;



class test_t: public map_t, timer_t {
public:
    test_t(): timer_t(500 *US_PER_MS) {}

    timer_t* get_timer() { return dynamic_cast<timer_t*>(this); }

    void on_press(pmap_t* ppmap) {
        start_timer(ppmap);
        start_defer_presses();
        pressing = true;
    }

    void on_release(pmap_t*) {
        stop_timer();
        stop_defer_presses();
        pressing = false;

        if ( FN.is_pressing() && ext_LCTL.is_pressing() )
            system_reset();
    }

    void on_timeout(pmap_t*) {
        if ( FN.is_pressing() ) {
            if ( ext_LCTL.is_pressing() )
                // assert( false );
                WDT->CLEAR.reg = 0;  // anything other than 0xA5
            else
                perform_usbhub_switchover();
        }
    }

    bool is_pressing() const { return pressing; }

private:
    bool pressing = false;
};

inline test_t TEST;



class ext_lsft_t: public map_t {
public:
    void on_press(pmap_t*) {
        m_code = ext_SPC.is_pressing() ? KC_SPACE : KC_LSHIFT;
        send_press(m_code);
    }

    void on_release(pmap_t*) {
        send_release(m_code);
        m_code = 0;
    }

    bool is_pressing() const { return m_code != 0; }

private:
    uint8_t m_code = 0;
};

inline ext_lsft_t ext_LSFT;



// Todo: Allocate them in PROGMEM(?).
pmap_t maps[MATRIX_ROWS][MATRIX_COLS] = {
    GRV, _1, _2, _3, _4, _5, _6, _7, _8, _9, _0, MINUS, EQL, BKSP, DEL,

    TAB, Q, W, E, R, T, Y, U, I, O, P, LBRKT, RBRKT, BSLASH, HOME,

    ext_LCTL, A, S, D, F, G, H, J, K, L, COLON, QUOTE, ____, ENT, PGUP,

    ext_LSFT, ____, Z, X, C, V, B, N, M, COMMA, DOT, SLASH, RSFT, UP, PGDN,

    FN, LGUI, LALT, ____, ____, ____, ext_SPC, ____, ____, ____,
        RCTL, TEST /*RALT*/, LEFT, DOWN, RIGHT,
};

}  // namespace key