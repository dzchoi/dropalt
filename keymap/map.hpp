#pragma once

#include <type_traits>          // for std::is_base_of_v<>
#include "adc_thread.hpp"       // for signal_extra_enable_manually(), ...
#include "keymap_thread.hpp"    // for signal_usbport_switchover()
#include "usb_thread.hpp"       // for hid_keyboard.report_press/release()



namespace key {

class map_lamp_t;
class map_proxy_t;
class pmap_t;



// The base class of all keymaps, which does not register any press or release, but can
// be only checked for its pressing.
class map_t {
public: // User-facing methods
    // Instances of map_t are not supposed to be const or constexpr since they contain
    // the mutable member, m_press_count. However, the constexpr contructor (especially
    // that in literal_t) still helps them to be created in .relocate section instead of
    // .bss section, which means their initialization is done at compile-time rather than
    // from __libc_init_array() at run-time.
    // See https://stackoverflow.com/questions/50149036/ for constexpr constructor for
    // non-const objects.
    constexpr map_t() =default;

    // Move-constructible
    constexpr map_t(map_t&&) =default;

    // Not copyable
    map_t(const map_t&) =delete;
    map_t& operator=(const map_t&) =delete;

    // Execute on_press/release() of the keymap with the given slot number that indicates
    // who has triggered the keymap. Beware to not call `pmap->on_press(slot)` directly,
    // which does not take care of simultaneous presses of the same keymap.
    void press(pmap_t* slot);
    void release(pmap_t* slot);

    // Indicate if the keymap (not the slot) is being pressed.
    bool is_pressed() const { return m_press_count > 0; }

private:
    int8_t m_press_count = 0;

    // Proxy keymaps will return their `map_proxy_t*` through this virtual method.
    virtual map_proxy_t* get_proxy() { return nullptr; }

    friend class map_proxy_t;  // for on_press/release()
    friend class pmap_t;  // for get_lamp()

    // Keymaps that implement map_lamp_t will return their map_lamp_t* through this
    // virtual method.
    virtual map_lamp_t* get_lamp() { return nullptr; }

    virtual void on_press(pmap_t*) {}
    virtual void on_release(pmap_t*) {}

protected: // Utility methods that can be used by child classes
    static void send_press(uint8_t keycode) {
        usb_thread::obj().hid_keyboard.report_press(keycode);
    }

    static void send_release(uint8_t keycode) {
        usb_thread::obj().hid_keyboard.report_release(keycode);
    }

    // Perform usbhub switchover, once all keys are released.
    static void perform_usbport_switchover() {
        keymap_thread::obj().signal_usbport_switchover();
    }

    static void enable_extra_usbport_manually() {
        adc_thread::obj().signal_extra_enable_manually();
    }

    static void set_extra_usbport_back_to_automatic() {
        adc_thread::obj().signal_extra_back_to_automatic();
    }
};

// Keys that do nothing
inline map_t NO;
inline map_t& ___ = NO;



// Meta-function to map T -> T and T& -> map_t&, if T is a derived class of map_t.

template <class T>
struct obj_or_ref {
    static_assert( std::is_base_of_v<map_t, T> );
    typedef T type;
};

template <class T>
struct obj_or_ref<T&> {
    static_assert( std::is_base_of_v<map_t, T> );
    typedef map_t& type;
};

template <class T>
using obj_or_ref_t = typename obj_or_ref<T>::type;

}  // namespace key
