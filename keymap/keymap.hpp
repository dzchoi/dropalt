#pragma once

#include "periph_conf.h"        // for MATRIX_ROWS and MATRIX_COLS

#include "adc_thread.hpp"       // for signal_usbhub_switchover()
#include "keymap_thread.hpp"    // for start_defer_presses() and stop_defer_presses()
#include "usb_thread.hpp"       // for report_press/release()



namespace key {

class pbase_t;
class timer_t;



// Base keymap that does nothing, not pressing nor releasing.
class base_t {
public:
    virtual void on_press(pbase_t*) {};

    virtual void on_release(pbase_t*) {};

    virtual bool is_pressing() const { return false; };

    // Child classes that also derive from key::timer_t (e.g. tap_hold_t) return their
    // key::timer_t* through this virtual method.
    // (Instead of implementing this explicit method we could make use of
    // dynamic_cast<timer_t*> along with RTTI embedded to every class, which would
    // require the binary size increased by several KBs.)
    virtual timer_t* get_timer() { return nullptr; }

protected:
    // Utility methods that can be used by child classes.
    void send_press(uint8_t keycode) {
        usb_thread::obj().hid_keyboard.report_press(keycode);
    }

    void send_release(uint8_t keycode) {
        usb_thread::obj().hid_keyboard.report_release(keycode);
    }

    void start_defer_presses() {
        keymap_thread::obj().start_defer_presses();
    }

    void stop_defer_presses() {
        keymap_thread::obj().stop_defer_presses();
    }

    void perform_usbhub_switchover() {
        // Todo:
        // Windows seems to have a problem. Take a long time to recognize the device.
        // Only capital letters type in Windows sometimes. CDC ACM is lost when going
        // back to the original port.
        adc_thread::obj().signal_usbhub_switchover();
    }

private:
    // Todo: link pointer to pressing keys list. This can help determining whether to
    //  report to maps[][] or not, also helps debouncing per key.
};

// Keys that do nothing
inline base_t NO;
inline base_t& ____ = NO;



// The pbase_t is the same as base_t*. It can be initialized from an lvalue of base_t, to
// save the typing of `&` when initializing maps[][]/
class pbase_t {
public:
    constexpr pbase_t(base_t& base): m_pbase(&base) {}
    base_t* operator->() { return m_pbase; }
    base_t& operator*() { return *m_pbase; }

    operator base_t*() { return m_pbase; }

private:
    base_t* const m_pbase;
};

// The "matrix" that all keymaps reside in; `pbase_t*` can serve as a unique "slot id"
// that is assigned to each keymap in it.
// Note that the keymaps are not identified by some 8-bit or 16-bit keycodes like in QMK,
// but by their own lvalue. The literal_t keymaps do have their keycode() but more
// complex types do not.
extern pbase_t maps[MATRIX_ROWS][MATRIX_COLS];

}  // namespace key
