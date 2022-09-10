#pragma once

#include <array>

#define class _class
// Riot's sys/include/usb/descriptor.h uses "class" as a struct member name.
#include "usb/usbus.h"          // Patch hid.h below.
#undef class
#include "usb/usbus/hid.h"      // for usbus_hid_device_t, usbus_hid_cb_t
#include "xtimer.h"             // for xtimer_t

#include "usb_descriptor.hpp"
#include "features.h"



struct usbus_hid_device_ext_t: usbus_hid_device_t {
    xtimer_t tx_timer;

    usbus_hid_device_ext_t()
    { memset(dynamic_cast<usbus_hid_device_t*>(this), 0, sizeof(usbus_hid_device_t)); }
    virtual ~usbus_hid_device_ext_t() {}

    virtual void pre_init(usbus_t* usbus,
        const uint8_t* report_desc, size_t report_desc_size, usbus_hid_cb_t cb);

    virtual void init(usbus_t* usbus) { (void)usbus; };

    void flush();

    virtual bool is_keyboard() const { return false; }

    static usbus_hid_device_ext_t& keyboard;
    size_t submit(const uint8_t* buf, size_t len);

    usbus_hid_device_ext_t(const usbus_hid_device_ext_t&) =delete;
    usbus_hid_device_ext_t& operator=(const usbus_hid_device_ext_t&) =delete;
};



// Simplified std::copy_n() but constexpr function.
template <typename T>
constexpr void _copy_n(const T* src, size_t n, T* dst)
{
    for ( size_t i = 0 ; i < n ; ++i )
        dst[i] = src[i];
}

// Converts C-arrays into a std::array<>.
// inspired by https://stackoverflow.com/a/66317066/10451825
template <typename T, size_t... Ns>
constexpr auto array_of(const T (&... arrs)[Ns])
{
    std::array<T, (0 + ... + Ns)> result {};
    size_t i = 0;
    ((_copy_n(arrs, Ns, result.data() + i), i += Ns), ...);
    return result;
}

struct usbus_hid_keyboard_t: usbus_hid_device_ext_t {
    static inline constexpr auto report_desc = array_of(KeyboardReport);

    void pre_init(usbus_t* usbus, usbus_hid_cb_t cb =nullptr)
#ifndef KEYBOARD_SHARED_EP
    { usbus_hid_device_ext_t::pre_init(usbus, report_desc.data(), report_desc.size(), cb); }
#else
    { (void)usbus; (void)cb; }
#endif

#ifndef KEYBOARD_SHARED_EP
    void init(usbus_t* usbus);

    bool is_keyboard() const { return true; }
#endif
};

struct usbus_hid_mouse_t: usbus_hid_device_ext_t {
    static inline constexpr auto report_desc = array_of(MouseReport);

    void pre_init(usbus_t* usbus, usbus_hid_cb_t cb =nullptr)
#if defined(MOUSE_ENABLE) && !defined(MOUSE_SHARED_EP)
    { usbus_hid_device_ext_t::pre_init(usbus, report_desc.data(), report_desc.size(), cb); }
#else
    { (void)usbus; (void)cb; }
#endif

#if defined(MOUSE_ENABLE) && !defined(MOUSE_SHARED_EP)
    void init(usbus_t* usbus);
#endif
};

struct usbus_hid_shared_t: usbus_hid_device_ext_t {
    static inline constexpr auto report_desc = array_of<uint8_t>(
    // TOdo: Dangling ',' can cause an error.
#ifdef KEYBOARD_SHARED_EP
        KeyboardReport
#endif
#if defined(MOUSE_ENABLE) && defined(MOUSE_SHARED_EP)
        ,MouseReport
#endif
#ifdef EXTRAKEY_ENABLE
        ,ExtrakeyReport
#endif
#ifdef NKRO_ENABLE
        ,NkroReport
#endif
    );

    void pre_init(usbus_t* usbus, usbus_hid_cb_t cb =nullptr)
#ifdef SHARED_EP_ENABLE
    { usbus_hid_device_ext_t::pre_init(usbus, report_desc.data(), report_desc.size(), cb); }
#else
    { (void)usbus; (void)cb; }
#endif

#ifdef SHARED_EP_ENABLE
    void init(usbus_t* usbus);
#endif

#ifdef KEYBOARD_SHARED_EP
    bool is_keyboard() const { return true; }
#endif
};

struct usbus_hid_raw_t: usbus_hid_device_ext_t {
    static inline constexpr auto report_desc = array_of(RawReport);

    void pre_init(usbus_t* usbus, usbus_hid_cb_t cb =nullptr)
#ifdef RAW_ENABLE
    { usbus_hid_device_ext_t::pre_init(usbus, report_desc.data(), report_desc.size(), cb); }
#else
    { (void)usbus; (void)cb; }
#endif

#ifdef RAW_ENABLE
    void init(usbus_t* usbus);
#endif
};

struct usbus_hid_console_t: usbus_hid_device_ext_t {
    static inline constexpr auto report_desc = array_of(ConsoleReport);

    void pre_init(usbus_t* usbus, usbus_hid_cb_t cb =nullptr)
#ifdef CONSOLE_ENABLE
    { usbus_hid_device_ext_t::pre_init(usbus, report_desc.data(), report_desc.size(), cb); }
#else
    { (void)usbus; (void)cb; }
#endif

#ifdef CONSOLE_ENABLE
    void init(usbus_t* usbus);
#endif
};
