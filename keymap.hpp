#pragma once

#include "periph_conf.h"        // for MATRIX_ROWS and MATRIX_COLS

#include "usb_thread.hpp"



class keymap_t {
public:
    virtual void operator()(bool pressed) =0;

private:
    // Todo: link pointer to unprocessed key list.
    // Todo: Attribute, e.g. to get notified of other key being pressed while holding.
};



class map_const_t: public keymap_t {
public:
    void operator()(bool pressed) {
        usb_thread::obj().hid_keyboard.key_report(m_code, pressed);
    }

    template <uint8_t CODE>
    static map_const_t& create(Key<CODE>) {
        static map_const_t obj(CODE);
        return obj;
    }

private:
    const uint8_t m_code;

    constexpr map_const_t(uint8_t code): m_code(code) {}
};



class rkeymap_t {
public:
    template <uint8_t CODE>
    constexpr rkeymap_t(Key<CODE> key): m_keymap(map_const_t::create(key)) {}

    constexpr rkeymap_t(keymap_t& keymap): m_keymap(keymap) {}

    // Todo: (For convenience) declare a (rvalue of) keymap_t in place.
    // rkeymap_t(keymap_t&& keymap): m_keymap(std::move(keymap)) {}

    void operator()(bool pressed) { m_keymap(pressed); }

private:
    keymap_t& m_keymap;
};



extern rkeymap_t keymap[MATRIX_ROWS][MATRIX_COLS];