#pragma once

#include "periph_conf.h"        // for MATRIX_ROWS and MATRIX_COLS

#include "keymap_thread.hpp"    // for start_defer_presses() and stop_defer_presses()
#include "usb_thread.hpp"       // for report_press/release()
#include "whole.hpp"            // for key::whole.execute_press/release()



namespace key {

class map_timer_t;
class pmap_t;



// The base class of all keymaps, which does not register any press or release, but can
// be only checked for its pressing.
class map_t {
public:
    virtual void on_press(pmap_t*) {};

    virtual void on_release(pmap_t*) {};

    // Indicate if the keymap (not the slot) is being pressed.
    bool is_pressing() const { return m_pressing_count != 0; };

    // Child classes that derive from map_timer_t (e.g. tap_hold_t) will return their
    // map_timer_t* through this virtual method.
    // (Instead of implementing this explicit method we could make use of
    // dynamic_cast<map_timer_t*> along with RTTI embedded to every class, which would
    // increase the binary size by several KBs.)
    virtual map_timer_t* get_timer() { return nullptr; }

protected:
    // Utility methods that can be used by child classes.

    void send_press(uint8_t keycode) {
        usb_thread::obj().hid_keyboard.report_press(keycode);
    }

    void send_release(uint8_t keycode) {
        usb_thread::obj().hid_keyboard.report_release(keycode);
    }

    // Execute on_press/release() of other keymap with the slot who triggers the keymap.
    // Beware to not call `pmap->on_press(slot)` directly, which does not take care of
    // simultaneous presses of the same keymap.
    void execute_press(map_t* pmap, pmap_t* slot) {
        key::whole.execute_press(pmap, slot);
    }

    void execute_release(map_t* pmap, pmap_t* slot) {
        key::whole.execute_release(pmap, slot);
    }

    // Start deferring key presses. All presses will be deferred from the next press
    // event.
    void start_defer_presses() {
        keymap_thread::obj().start_defer_presses();
    }

    // Stop deferring key presses. All the deferred presses will be carried out after
    // handling the current event, i.e. when the current event-handling method (on_*())
    // that was calling stop_defer_presses() finishes.
    void stop_defer_presses() {
        keymap_thread::obj().stop_defer_presses();
    }

    // Perform usbhub switchover, once all keys are released.
    void perform_usbhub_switchover() {
        keymap_thread::obj().signal_usbhub_switchover();
    }

private:
    friend class whole_t;
    uint8_t m_pressing_count = 0;
};

// Keys that do nothing
inline map_t NO;
inline map_t& ___ = NO;

}  // namespace key
