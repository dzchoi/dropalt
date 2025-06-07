#pragma once

#include <array>
#include <cstddef>              // for size_t
#include <cstdint>              // for uint8_t

#define CONCAT(x, y) x##y
#define CONCAT_EXPANDED(x, y) CONCAT(x, y)
#include "_HIDReportData.h"


// HID report IDs, used only for Shared EP
enum hid_report_ids {
    REPORT_ID_KEYBOARD = 1,
    REPORT_ID_MOUSE,
    REPORT_ID_SYSTEM,
    REPORT_ID_CONSUMER,
    REPORT_ID_NKRO,
    REPORT_ID_JOYSTICK
};


/*
 * HID report descriptors
 */

constexpr size_t SKRO_REPORT_SIZE = 8;
constexpr size_t SKRO_KEYS_SIZE = SKRO_REPORT_SIZE - 2;
// So, 6 keys can be reported simultaneously.

inline constexpr uint8_t SkroReportDescriptor[] = {
    HID_RI_USAGE_PAGE(8, 0x01),        // Generic Desktop
    HID_RI_USAGE(8, 0x06),             // Keyboard
    HID_RI_COLLECTION(8, 0x01),        // Application
        // HID_RI_REPORT_ID(8, REPORT_ID_KEYBOARD),  // for Shared EP
        // Modifiers (8 bits)
        HID_RI_USAGE_PAGE(8, 0x07),    // Keyboard/Keypad
        HID_RI_USAGE_MINIMUM(8, 0xE0), // Keyboard Left Control
        HID_RI_USAGE_MAXIMUM(8, 0xE7), // Keyboard Right GUI
        HID_RI_LOGICAL_MINIMUM(8, 0x00),
        HID_RI_LOGICAL_MAXIMUM(8, 0x01),
        HID_RI_REPORT_COUNT(8, 0x08),
        HID_RI_REPORT_SIZE(8, 0x01),
        HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),
        // Reserved (1 byte)
        HID_RI_REPORT_COUNT(8, 0x01),
        HID_RI_REPORT_SIZE(8, 0x08),
        HID_RI_INPUT(8, HID_IOF_CONSTANT),
        // Keycodes (6 bytes)
        HID_RI_USAGE_PAGE(8, 0x07),    // Keyboard/Keypad
        HID_RI_USAGE_MINIMUM(8, 0x00),
        HID_RI_USAGE_MAXIMUM(8, 0xFF),
        HID_RI_LOGICAL_MINIMUM(8, 0x00),
        // This Maximum value should not be 16-bit 0x00FF as is in QMK, or Windows won't
        // be able to recognize the device though Linux seems fine with it.
        HID_RI_LOGICAL_MAXIMUM(8, 0xFF),
        HID_RI_REPORT_COUNT(8, SKRO_KEYS_SIZE),
        HID_RI_REPORT_SIZE(8, 0x08),
        HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_ARRAY | HID_IOF_ABSOLUTE),

        // Status LEDs (5 bits)
        HID_RI_USAGE_PAGE(8, 0x08),    // LED
        HID_RI_USAGE_MINIMUM(8, 0x01), // Num Lock
        HID_RI_USAGE_MAXIMUM(8, 0x05), // Kana
        HID_RI_REPORT_COUNT(8, 0x05),
        HID_RI_REPORT_SIZE(8, 0x01),
        HID_RI_OUTPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE | HID_IOF_NON_VOLATILE),
        // LED padding (3 bits)
        HID_RI_REPORT_COUNT(8, 0x01),
        HID_RI_REPORT_SIZE(8, 0x03),
        HID_RI_OUTPUT(8, HID_IOF_CONSTANT),
    HID_RI_END_COLLECTION(0)
};

constexpr size_t NKRO_REPORT_SIZE = 32;  // must be 8, 16, 32 or 64.
constexpr size_t NKRO_KEYS_SIZE = NKRO_REPORT_SIZE - 1;  // == 31 bytes
// So, 248 (= 31*8 = 0xF8) keys can be reported.

inline constexpr uint8_t NkroReportDescriptor[] = {
    HID_RI_USAGE_PAGE(8, 0x01),        // Generic Desktop
    HID_RI_USAGE(8, 0x06),             // Keyboard
    HID_RI_COLLECTION(8, 0x01),        // Application
        // HID_RI_REPORT_ID(8, REPORT_ID_NKRO),  // for Shared EP
        // Modifiers (8 bits)
        HID_RI_USAGE_PAGE(8, 0x07),    // Keyboard/Keypad
        HID_RI_USAGE_MINIMUM(8, 0xE0), // Keyboard Left Control
        HID_RI_USAGE_MAXIMUM(8, 0xE7), // Keyboard Right GUI
        HID_RI_LOGICAL_MINIMUM(8, 0x00),
        HID_RI_LOGICAL_MAXIMUM(8, 0x01),
        HID_RI_REPORT_COUNT(8, 0x08),
        HID_RI_REPORT_SIZE(8, 0x01),
        HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),
        // Keycodes
        HID_RI_USAGE_PAGE(8, 0x07),    // Keyboard/Keypad
        HID_RI_USAGE_MINIMUM(8, 0x00),
        HID_RI_USAGE_MAXIMUM(8, NKRO_KEYS_SIZE * 8 - 1),
        HID_RI_LOGICAL_MINIMUM(8, 0x00),
        HID_RI_LOGICAL_MAXIMUM(8, 0x01),
        HID_RI_REPORT_COUNT(8, NKRO_KEYS_SIZE * 8),
        HID_RI_REPORT_SIZE(8, 0x01),
        HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),

        // Status LEDs (5 bits)
        HID_RI_USAGE_PAGE(8, 0x08),    // LED
        HID_RI_USAGE_MINIMUM(8, 0x01), // Num Lock
        HID_RI_USAGE_MAXIMUM(8, 0x05), // Kana
        HID_RI_REPORT_COUNT(8, 0x05),
        HID_RI_REPORT_SIZE(8, 0x01),
        HID_RI_OUTPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE | HID_IOF_NON_VOLATILE),
        // LED padding (3 bits)
        HID_RI_REPORT_COUNT(8, 0x01),
        HID_RI_REPORT_SIZE(8, 0x03),
        HID_RI_OUTPUT(8, HID_IOF_CONSTANT),
    HID_RI_END_COLLECTION(0)
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

// Not used.
template <typename T, size_t N>
constexpr auto remove_report_id(const std::array<T, N>& arr)
{
    // Due to the current limitation of C++ that cannot specify the size of std::array<>
    // with an implicit constexpr value in a constexpr function, this function can remove
    // only the HID_RI_REPORT_ID(8, x) which occupies 2 bytes.
    // size_t n = 0;
    // size_t i = 0;
    // for ( i = 0 ; i < N ; ++i ) {
    //     if ( (arr[i] & ~0x3u) == HID_RI_REPORT_ID(0) ) {  // i.e. == 0x84
    //         n = (arr[i] & 0x3u) + 1;  // +1 to include the Type byte.
    //         break;
    //     }
    // }

    constexpr size_t n = 2;
    size_t i = 0;
    for ( i = 0 ; i < N ; ++i ) {
        if ( arr[i] == HID_RI_REPORT_ID(0) + 1 )  // i.e. == 0x85
            break;
    }

    // arr[i, i+n) is the bytes containing the report id.

    std::array<T, N - n> result {};
    size_t j = 0;
    for ( j = 0 ; j < i ; ++j )
        result[j] = arr[j];
    for ( j = i + n ; j < N ; ++i, ++j )
        result[i] = arr[j];

    return result;
}
