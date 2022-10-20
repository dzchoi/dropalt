#pragma once

#include <stddef.h>             // for size_t
#include <stdint.h>             // for uint8_t

#include <array>

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

constexpr uint8_t SkroReportDescriptor[] = {
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

constexpr uint8_t NkroReportDescriptor[] = {
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

// For "hid_listen"
constexpr uint16_t RAW_USAGE_PAGE = 0xFF31u;  // PJRC Teensy compatible
constexpr uint8_t RAW_USAGE_ID = 0x74;        // PJRC Teensy compatible

// For ...???
// constexpr uint16_t RAW_USAGE_PAGE = 0xFF60u;
// constexpr uint8_t RAW_USAGE_ID = 0x61;

constexpr uint8_t RAW_USAGE_PAGE_HI = (uint8_t)(RAW_USAGE_PAGE >> 8);
constexpr uint8_t RAW_USAGE_PAGE_LO = (uint8_t)(RAW_USAGE_PAGE & 0xFF);

constexpr size_t RAW_REPORT_SIZE = 64;  // must be 8, 16, 32 or 64.

constexpr uint8_t RawReportDescriptor[] = {
    HID_RI_USAGE_PAGE(16, RAW_USAGE_PAGE), // Vendor Defined
    HID_RI_USAGE(8, RAW_USAGE_ID),         // Vendor Defined
    HID_RI_COLLECTION(8, 0x01),    // Application
        // Data to host
        HID_RI_USAGE(8, RAW_USAGE_ID + 1), // Vendor Defined
        HID_RI_LOGICAL_MINIMUM(8, 0x00),
        HID_RI_LOGICAL_MAXIMUM(8, 0xFF),
        HID_RI_REPORT_COUNT(8, RAW_REPORT_SIZE),
        HID_RI_REPORT_SIZE(8, 0x08),
        HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),

        // Data from host
        HID_RI_USAGE(8, RAW_USAGE_ID + 2), // Vendor Defined
        HID_RI_LOGICAL_MINIMUM(8, 0x00),
        HID_RI_LOGICAL_MAXIMUM(8, 0xFF),
        HID_RI_REPORT_COUNT(8, RAW_REPORT_SIZE),
        HID_RI_REPORT_SIZE(8, 0x08),
        HID_RI_OUTPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE | HID_IOF_NON_VOLATILE),
    HID_RI_END_COLLECTION(0)
};

/*
constexpr uint8_t MouseReportDescriptor[] = {
    HID_RI_USAGE_PAGE(8, 0x01),            // Generic Desktop
    HID_RI_USAGE(8, 0x02),                 // Mouse
    HID_RI_COLLECTION(8, 0x01),            // Application
        // HID_RI_REPORT_ID(8, REPORT_ID_MOUSE),  // for Shared EP
        HID_RI_USAGE(8, 0x01),             // Pointer
        HID_RI_COLLECTION(8, 0x00),        // Physical
            // Buttons (8 bits)
            HID_RI_USAGE_PAGE(8, 0x09),    // Button
            HID_RI_USAGE_MINIMUM(8, 0x01), // Button 1
            HID_RI_USAGE_MAXIMUM(8, 0x08), // Button 8
            HID_RI_LOGICAL_MINIMUM(8, 0x00),
            HID_RI_LOGICAL_MAXIMUM(8, 0x01),
            HID_RI_REPORT_COUNT(8, 0x08),
            HID_RI_REPORT_SIZE(8, 0x01),
            HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),

            // X/Y position (2 bytes)
            HID_RI_USAGE_PAGE(8, 0x01),    // Generic Desktop
            HID_RI_USAGE(8, 0x30),         // X
            HID_RI_USAGE(8, 0x31),         // Y
            HID_RI_LOGICAL_MINIMUM(8, -127),
            HID_RI_LOGICAL_MAXIMUM(8, 127),
            HID_RI_REPORT_COUNT(8, 0x02),
            HID_RI_REPORT_SIZE(8, 0x08),
            HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_RELATIVE),

            // Vertical wheel (1 byte)
            HID_RI_USAGE(8, 0x38),         // Wheel
            HID_RI_LOGICAL_MINIMUM(8, -127),
            HID_RI_LOGICAL_MAXIMUM(8, 127),
            HID_RI_REPORT_COUNT(8, 0x01),
            HID_RI_REPORT_SIZE(8, 0x08),
            HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_RELATIVE),
            // Horizontal wheel (1 byte)
            HID_RI_USAGE_PAGE(8, 0x0C),    // Consumer
            HID_RI_USAGE(16, 0x0238),      // AC Pan
            HID_RI_LOGICAL_MINIMUM(8, -127),
            HID_RI_LOGICAL_MAXIMUM(8, 127),
            HID_RI_REPORT_COUNT(8, 0x01),
            HID_RI_REPORT_SIZE(8, 0x08),
            HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_RELATIVE),
        HID_RI_END_COLLECTION(0),
    HID_RI_END_COLLECTION(0)
};

constexpr uint8_t ExtrakeyReportDescriptor[] = {
    HID_RI_USAGE_PAGE(8, 0x01),           // Generic Desktop
    HID_RI_USAGE(8, 0x80),                // System Control
    HID_RI_COLLECTION(8, 0x01),           // Application
        // HID_RI_REPORT_ID(8, REPORT_ID_SYSTEM),  // for Shared EP
        HID_RI_USAGE_MINIMUM(8, 0x01),    // Pointer
        HID_RI_USAGE_MAXIMUM(16, 0x00B7), // System Display LCD Autoscale
        HID_RI_LOGICAL_MINIMUM(8, 0x01),
        HID_RI_LOGICAL_MAXIMUM(16, 0x00B7),
        HID_RI_REPORT_COUNT(8, 1),
        HID_RI_REPORT_SIZE(8, 16),
        HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_ARRAY | HID_IOF_ABSOLUTE),
    HID_RI_END_COLLECTION(0),

    HID_RI_USAGE_PAGE(8, 0x0C),           // Consumer
    HID_RI_USAGE(8, 0x01),                // Consumer Control
    HID_RI_COLLECTION(8, 0x01),           // Application
        // HID_RI_REPORT_ID(8, REPORT_ID_CONSUMER),  // for Shared EP
        HID_RI_USAGE_MINIMUM(8, 0x01),    // Consumer Control
        HID_RI_USAGE_MAXIMUM(16, 0x02A0), // AC Desktop Show All Applications
        HID_RI_LOGICAL_MINIMUM(8, 0x01),
        HID_RI_LOGICAL_MAXIMUM(16, 0x02A0),
        HID_RI_REPORT_COUNT(8, 1),
        HID_RI_REPORT_SIZE(8, 16),
        HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_ARRAY | HID_IOF_ABSOLUTE),
    HID_RI_END_COLLECTION(0)
};

constexpr uint8_t JoystickReportDescriptor[] = {
#ifdef JOYSTICK_ENABLE
#if JOYSTICK_AXES_COUNT == 0 && JOYSTICK_BUTTON_COUNT == 0
#   error Need at least one axis or button for joystick
#endif
#endif
    HID_RI_USAGE_PAGE(8, 0x01),         // Generic Desktop
    HID_RI_USAGE(8, 0x04),              // Joystick
    HID_RI_COLLECTION(8, 0x01),         // Application
        HID_RI_COLLECTION(8, 0x00),     // Physical
            HID_RI_USAGE_PAGE(8, 0x01), // Generic Desktop
#   if JOYSTICK_AXES_COUNT >= 1
            HID_RI_USAGE(8, 0x30),      // X
#   endif
#   if JOYSTICK_AXES_COUNT >= 2
            HID_RI_USAGE(8, 0x31),      // Y
#   endif
#   if JOYSTICK_AXES_COUNT >= 3
            HID_RI_USAGE(8, 0x32),      // Z
#   endif
#   if JOYSTICK_AXES_COUNT >= 4
            HID_RI_USAGE(8, 0x33),      // Rx
#   endif
#   if JOYSTICK_AXES_COUNT >= 5
            HID_RI_USAGE(8, 0x34),      // Ry
#   endif
#   if JOYSTICK_AXES_COUNT >= 6
            HID_RI_USAGE(8, 0x35),      // Rz
#   endif
#   if JOYSTICK_AXES_COUNT >= 1
    # if JOYSTICK_AXES_RESOLUTION == 8
            HID_RI_LOGICAL_MINIMUM(8, -JOYSTICK_RESOLUTION),
            HID_RI_LOGICAL_MAXIMUM(8, JOYSTICK_RESOLUTION),
            HID_RI_REPORT_COUNT(8, JOYSTICK_AXES_COUNT),
            HID_RI_REPORT_SIZE(8, 0x08),
    # else
            HID_RI_LOGICAL_MINIMUM(16, -JOYSTICK_RESOLUTION),
            HID_RI_LOGICAL_MAXIMUM(16, JOYSTICK_RESOLUTION),
            HID_RI_REPORT_COUNT(8, JOYSTICK_AXES_COUNT),
            HID_RI_REPORT_SIZE(8, 0x10),
    # endif
            HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),
#   endif

#   if JOYSTICK_BUTTON_COUNT >= 1
            HID_RI_USAGE_PAGE(8, 0x09), // Button
            HID_RI_USAGE_MINIMUM(8, 0x01),
            HID_RI_USAGE_MAXIMUM(8, JOYSTICK_BUTTON_COUNT),
            HID_RI_LOGICAL_MINIMUM(8, 0x00),
            HID_RI_LOGICAL_MAXIMUM(8, 0x01),
            HID_RI_REPORT_COUNT(8, JOYSTICK_BUTTON_COUNT),
            HID_RI_REPORT_SIZE(8, 0x01),
            HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),

#       if (JOYSTICK_BUTTON_COUNT % 8) != 0
            HID_RI_REPORT_COUNT(8, 8 - (JOYSTICK_BUTTON_COUNT % 8)),
            HID_RI_REPORT_SIZE(8, 0x01),
            HID_RI_INPUT(8, HID_IOF_CONSTANT),
#       endif
#   endif
        HID_RI_END_COLLECTION(0),
    HID_RI_END_COLLECTION(0)
};
*/

#if 0
constexpr uint8_t report_desc_con[] = {
    0x06, 0x31, 0xFF,   // Usage Page (Vendor Defined - PJRC Teensy compatible)
    0x09, 0x74,         // Usage (Vendor Defined - PJRC Teensy compatible)
    0xA1, 0x01,         // Collection (Application)
    // Data to host
    0x09, 0x75,             // Usage (Vendor Defined)
    0x15, 0x00,             // Logical Minimum (0x00)
    0x26, 0xFF, 0x00,       // Logical Maximum (0x00FF)
    0x95, CONSOLE_EPSIZE,   // Report Count
    0x75, 0x08,             // Report Size (8)
    0x81, 0x02,             // Input (Data, Variable, Absolute)
    // Data from host
    0x09, 0x76,             // Usage (Vendor Defined)
    0x15, 0x00,             // Logical Minimum (0x00)
    0x26, 0xFF, 0x00,       // Logical Maximum (0x00FF)
    0x95, CONSOLE_EPSIZE,   // Report Count
    0x75, 0x08,             // Report Size (8)
    0x91, 0x02,             // Output (Data)
    0xC0                // End Collection
};

constexpr uint8_t report_desc_raw[] = {
    0x06, 0x60, 0xFF,   // Usage Page (Vendor Defined)
    0x09, 0x61,         // Usage (Vendor Defined)
    0xA1, 0x01,         // Collection (Application)
    0x75, 0x08,         //     Report Size (8)
    0x15, 0x00,         //     Logical Minimum (0)
    0x25, 0xFF,         //     Logical Maximum (255)
    // Data to host
    0x09, 0x62,         //     Usage (Vendor Defined)
    0x95, RAW_EPSIZE,   //     Report Count
    0x81, 0x02,         //     Input (Data, Variable, Absolute)
    // Data from host
    0x09, 0x63,         //     Usage (Vendor Defined)
    0x95, RAW_EPSIZE,   //     Report Count
    0x91, 0x02,         //     Output (Data, Variable, Absolute)
    0xC0                // End Collection
};

constexpr uint8_t CtapReportDescriptor[] = {
    // this descriptor is used, because the basic usb_hid interface was developed in
    // conjunction with FIDO2. Descriptor is taken from CTAP2 specification
    // (version 20190130) section 8.1.8.2
    0x06, 0xD0, 0xF1,   // HID_UsagePage ( FIDO_USAGE_PAGE )
    0x09, 0x01,         // HID_Usage ( FIDO_USAGE_CTAPHID )
    0xA1, 0x01,         // HID_Collection ( HID_Application )
    0x09, 0x20,         // HID_Usage ( FIDO_USAGE_DATA_IN )
    0x15, 0x00,         // HID_LogicalMin ( 0 )
    0x26, 0xFF, 0x00,   // HID_LogicalMaxS ( 0xff )
    0x75, 0x08,         // HID_ReportSize ( 8 )
    0x95, 0x40,         // HID_ReportCount ( HID_INPUT_REPORT_BYTES )
    0x81, 0x02,         // HID_Input ( HID_Data | HID_Absolute | HID_Variable )
    0x09, 0x21,         // HID_Usage ( FIDO_USAGE_DATA_OUT )
    0x15, 0x00,         // HID_LogicalMin ( 0 )
    0x26, 0xFF, 0x00,   // HID_LogicalMaxS ( 0xff )
    0x75, 0x08,         // HID_ReportSize ( 8 )
    0x95, 0x40,         // HID_ReportCount ( HID_OUTPUT_REPORT_BYTES )
    0x91, 0x02,         // HID_Output ( HID_Data | HID_Absolute | HID_Variable )
    0xC0,               // HID_EndCollection
};
#endif



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
