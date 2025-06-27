#pragma once

#include "usb_thread.hpp"       // for usb_thread::send_press/release()



struct lua_State;

namespace lua {

namespace key {

class map_proxy_t;

// Base class for all keymaps.
// The base class itself can handle press and release events virtually, but does not
// register keys actually.
class map_t {
public: // User-facing methods callable from Lua
    // Constructor in Lua:
    // Each map_t class, along with its derived classes, defines a creator function
    // `_create()`, callable from Lua (e.g., `map_literal(keycode)`), which creates a
    // keymap object as userdata on the Lua stack. Since these objects are created at
    // runtime, invoking C++ constructors at compile time is not allowed.
    static int _create(lua_State* L);

    // Implicit destructor:
    // __gc() invokes the _delete() method defined by the appropriate map_t-derived
    // class.
    // Note: This is necessary because derived classes cannot invoke their overridden
    // destructors via a call to ~map_t() alone.
    virtual void _delete() { this->~map_t(); }

    // Execute the press/release actions by invoking on_press/release() on the
    // appropriate derived class instance.
    // Also accessible from Lua using `keymap:press/release()`.
    void press();
    void release();

    // Return true if this keymap object (not the slot) is currently pressed.
    // Also accessible from Lua using `keymap:is_pressed()`.
    bool is_pressed() const { return m_press_count > 0; }

protected:
    explicit map_t() =default;  // Only _create() can create an instance.

    virtual ~map_t();  // Only _delete() can destroy the instance.

private:
    int8_t m_press_count = 0;

    // Proxy keymaps will return their `map_proxy_t*` using this virtual method.
    virtual map_proxy_t* get_proxy() { return nullptr; }

    // friend class map_proxy_t;  // for on_press/release()

    // Note: Avoid calling on_press/release() directly on keymap objects. These methods
    // don't handle simultaneous press/release events properly. Use .press/release()
    // instead.
    virtual void on_press() {}
    virtual void on_release() {}

protected: // Utility methods that can be used by derived classes
    static void send_press(uint8_t keycode) { usb_thread::send_press(keycode); }
    static void send_release(uint8_t keycode) { usb_thread::send_release(keycode); }

    // Retrieve map_t* from a Lua reference.
    static map_t* deref(int ref);
};

}

}
