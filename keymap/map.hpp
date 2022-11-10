#pragma once

#include "keymap_thread.hpp"    // for start/stop_defer_presses()
#include "manager.hpp"          // for key::manager.execute_press/release()
#include "usb_thread.hpp"       // for hid_keyboard.report_press/release()



namespace key {

class map_proxy_t;
class pmap_t;



// The base class of all keymaps, which does not register any press or release, but can
// be only checked for its pressing.
class map_t {
public: // User-facing methods
    constexpr map_t() =default;  // could be omitted.

    // Execute on_press/release() of the keymap with the slot who triggers the keymap.
    // Beware to not call `pmap->on_press(slot)` directly, which does not take care of
    // simultaneous presses of the same keymap.
    void press(pmap_t* slot) { key::manager.execute_press(this, slot); }

    void release(pmap_t* slot) { key::manager.execute_release(this, slot); }

    // Indicate if the keymap (not the slot) is being pressed.
    bool is_pressing() const { return m_pressing_count != 0; };

private: // Methods to be called by key::manager
    friend class manager_t;
    friend class map_proxy_t;

    // Proxy keymaps will return their map_proxy_t* through this virtual method.
    virtual map_proxy_t* get_proxy() { return nullptr; }

    virtual void on_press(pmap_t*) {};

    virtual void on_release(pmap_t*) {};

    uint8_t m_pressing_count = 0;

protected: // Utility methods that can be used by child classes
    static void send_press(uint8_t keycode) {
        usb_thread::obj().hid_keyboard.report_press(keycode);
    }

    static void send_release(uint8_t keycode) {
        usb_thread::obj().hid_keyboard.report_release(keycode);
    }

    // Start deferring key presses. All presses will be deferred from the next press
    // event.
    static void start_defer_presses() {
        keymap_thread::obj().start_defer_presses();
    }

    // Stop deferring key presses. All the deferred presses will be carried out after
    // handling the current event, i.e. when the current event-handling method (on_*())
    // that was calling stop_defer_presses() finishes.
    static void stop_defer_presses() {
        keymap_thread::obj().stop_defer_presses();
    }

    // Perform usbhub switchover, once all keys are released.
    static void perform_usbhub_switchover() {
        keymap_thread::obj().signal_usbhub_switchover();
    }
};

// Keys that do nothing
inline map_t NO;
inline map_t& ___ = NO;

}  // namespace key
