#pragma once

#include "periph_conf.h"        // for MATRIX_ROWS and MATRIX_COLS

#include "usb_thread.hpp"       // for report_press/release()



namespace key {

class pmap_t;
class timer_t;



class map_t {
public:
    virtual void on_press(pmap_t* ppmap) =0;

    virtual void on_release(pmap_t* ppmap) =0;

    virtual bool is_pressing() const =0;

    // Child classes that also derive from key::timer_t (e.g. tap_hold_t) return their
    // key::timer_t* through this virtual method.
    // (Instead of implementing this explicit method we could use dynamic_cast<timer_t*>
    // along with RTTI embedded to every class, which would require the binary size
    // increased by several KBs.)
    virtual timer_t* get_timer() { return nullptr; }

protected:
    // Utility methods that can be used by child classes.
    void send_press(uint8_t keycode) {
        usb_thread::obj().hid_keyboard.report_press(keycode);
    }

    void send_release(uint8_t keycode) {
        usb_thread::obj().hid_keyboard.report_release(keycode);
    }

private:
    // Todo: link pointer to pressing keys list. This can help determining whether to
    //  report to maps[] or not, also helps debouncing per key.
};



class pmap_t {
public:
    constexpr pmap_t(map_t& map): m_pmap(&map) {}
    map_t* operator->() { return m_pmap; }
    operator map_t*() { return m_pmap; }

private:
    map_t* const m_pmap;
};

// The "matrix" that all keys reside in; `pmap_t*` can serve as a unique "slot id" that
// is assigned to each key in it. Note that the keys are not identified by the 8-bit or
// 16-bit keycodes like in QMK, but by `pmap_t*`.
extern pmap_t maps[MATRIX_ROWS][MATRIX_COLS];

}  // namespace key
