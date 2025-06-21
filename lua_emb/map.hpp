#pragma once

#include "usb_thread.hpp"       // for usb_thread::send_press/release()



struct lua_State;

namespace lua {

namespace key {

class map_proxy_t;

// Base class for all keymaps. It handles press and release events virtually, but does
// not register keys directly.
class map_t {
public: // User-facing methods callable from Lua
    // Constructor in Lua:
    // Each map_t and its derived class defines a factory function `_create()`, callable
    // from Lua (e.g. `map_literal(keycode)`), which creates the instance as a userdata
    // on the Lua stack. As these instances are created at runtime, invoking C++
    // constructors at compile time is prohibited.
    // ( -- instance )
    static int _create(lua_State* L);

    // Destructor in Lua:
    // Each map_t and its derived class defines its own `__gc()` method, automatically
    // called by Lua during garbage collection to invoke the corresponding C++ destructor.
    // ( instance -- )
    static int __gc(lua_State* L);

    // Execute on_press/release() on the keymap instance.
    // Can also be invoked from Lua as `keymap:press/release()`.
    void press(unsigned slot_index);
    void release(unsigned slot_index);

    // Indicate if the keymap (not the slot) is being pressed.
    // Can also be invoked from Lua as `keymap:is_pressed()`.
    bool is_pressed() const { return m_press_count > 0; }

protected:
    explicit map_t() =default;  // Only _create() can create an instance.

    virtual ~map_t() =default;  // Only __gc() can destroy the instance.

private:
    int8_t m_press_count = 0;

    // Proxy keymaps will return their `map_proxy_t*` using this virtual method.
    virtual map_proxy_t* get_proxy() { return nullptr; }

    // friend class map_proxy_t;  // for on_press/release()

    // Note: Avoid calling .on_press() or .on_release() directly on instances, as they
    // don't properly handle simultaneous keymap press/release events. Use .press() and
    // .release() instead.
    virtual void on_press(unsigned) {}
    virtual void on_release(unsigned) {}

protected: // Utility methods that can be used by derived classes
    static void send_press(uint8_t keycode) { usb_thread::send_press(keycode); }
    static void send_release(uint8_t keycode) { usb_thread::send_release(keycode); }

    // Retrieve map_t* from a Lua reference.
    static map_t* deref(int ref);
};

}

}
