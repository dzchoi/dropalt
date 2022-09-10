#include "board.h"              // for CONSOLE_EPSIZE
#include "features.h"

#define CONCAT(x, y) x##y
#define CONCAT_EXPANDED(x, y) CONCAT(x, y)
#include "_HIDReportData.h"

#ifndef RAW_USAGE_PAGE
#    define RAW_USAGE_PAGE 0xFF60
#endif

#ifndef RAW_USAGE_ID
#    define RAW_USAGE_ID 0x61
#endif

#define RAW_USAGE_PAGE_HI ((uint8_t)(RAW_USAGE_PAGE >> 8))
#define RAW_USAGE_PAGE_LO ((uint8_t)(RAW_USAGE_PAGE & 0xFF))

/* HID report IDs */
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

inline constexpr uint8_t KeyboardReport[] = {
    HID_RI_USAGE_PAGE(8, 0x01),        // Generic Desktop
    HID_RI_USAGE(8, 0x06),             // Keyboard
    HID_RI_COLLECTION(8, 0x01),        // Application
#ifdef KEYBOARD_SHARED_EP
        HID_RI_REPORT_ID(8, REPORT_ID_KEYBOARD),
#endif
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
        // This maximum value should not be the 16-bit 0x00FF as is in QMK, or Windows
        // won't be able to recognize the keyboard (e.g. when NKRO_ENABLE=no and
        // KEYBOARD_SHARED_EP=no,) thought Linux seems fine with it.
        HID_RI_LOGICAL_MAXIMUM(8, 0xFF),
        HID_RI_REPORT_COUNT(8, 0x06),
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

inline constexpr uint8_t MouseReport[] = {
    HID_RI_USAGE_PAGE(8, 0x01),            // Generic Desktop
    HID_RI_USAGE(8, 0x02),                 // Mouse
    HID_RI_COLLECTION(8, 0x01),            // Application
#   ifdef MOUSE_SHARED_EP
        HID_RI_REPORT_ID(8, REPORT_ID_MOUSE),
#   endif
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

inline constexpr uint8_t ExtrakeyReport[] = {
    HID_RI_USAGE_PAGE(8, 0x01),           // Generic Desktop
    HID_RI_USAGE(8, 0x80),                // System Control
    HID_RI_COLLECTION(8, 0x01),           // Application
        HID_RI_REPORT_ID(8, REPORT_ID_SYSTEM),
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
        HID_RI_REPORT_ID(8, REPORT_ID_CONSUMER),
        HID_RI_USAGE_MINIMUM(8, 0x01),    // Consumer Control
        HID_RI_USAGE_MAXIMUM(16, 0x02A0), // AC Desktop Show All Applications
        HID_RI_LOGICAL_MINIMUM(8, 0x01),
        HID_RI_LOGICAL_MAXIMUM(16, 0x02A0),
        HID_RI_REPORT_COUNT(8, 1),
        HID_RI_REPORT_SIZE(8, 16),
        HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_ARRAY | HID_IOF_ABSOLUTE),
    HID_RI_END_COLLECTION(0)
};

inline constexpr uint8_t NkroReport[] = {
    HID_RI_USAGE_PAGE(8, 0x01),        // Generic Desktop
    HID_RI_USAGE(8, 0x06),             // Keyboard
    HID_RI_COLLECTION(8, 0x01),        // Application
        HID_RI_REPORT_ID(8, REPORT_ID_NKRO),
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
        HID_RI_USAGE_MAXIMUM(8, KEYBOARD_REPORT_BITS * 8 - 1),
        HID_RI_LOGICAL_MINIMUM(8, 0x00),
        HID_RI_LOGICAL_MAXIMUM(8, 0x01),
        HID_RI_REPORT_COUNT(8, KEYBOARD_REPORT_BITS * 8),
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

inline constexpr uint8_t RawReport[] = {
    HID_RI_USAGE_PAGE(16, RAW_USAGE_PAGE), // Vendor Defined
    HID_RI_USAGE(8, RAW_USAGE_ID),         // Vendor Defined
    HID_RI_COLLECTION(8, 0x01),    // Application
        // Data to host
        HID_RI_USAGE(8, 0x62),     // Vendor Defined
        HID_RI_LOGICAL_MINIMUM(8, 0x00),
        HID_RI_LOGICAL_MAXIMUM(8, 0xFF),
        HID_RI_REPORT_COUNT(8, RAW_EPSIZE),
        HID_RI_REPORT_SIZE(8, 0x08),
        HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),

        // Data from host
        HID_RI_USAGE(8, 0x63),     // Vendor Defined
        HID_RI_LOGICAL_MINIMUM(8, 0x00),
        HID_RI_LOGICAL_MAXIMUM(8, 0xFF),
        HID_RI_REPORT_COUNT(8, RAW_EPSIZE),
        HID_RI_REPORT_SIZE(8, 0x08),
        HID_RI_OUTPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE | HID_IOF_NON_VOLATILE),
    HID_RI_END_COLLECTION(0)
};

inline constexpr uint8_t ConsoleReport[] = {
    HID_RI_USAGE_PAGE(16, 0xFF31), // Vendor Defined (PJRC Teensy compatible)
    HID_RI_USAGE(8, 0x74),         // Vendor Defined (PJRC Teensy compatible)
    HID_RI_COLLECTION(8, 0x01),    // Application
        // Data to host
        HID_RI_USAGE(8, 0x75),     // Vendor Defined
        HID_RI_LOGICAL_MINIMUM(8, 0x00),
        HID_RI_LOGICAL_MAXIMUM(8, 0xFF),
        HID_RI_REPORT_COUNT(8, CONSOLE_EPSIZE),
        HID_RI_REPORT_SIZE(8, 0x08),
        HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),

        // Data from host
        HID_RI_USAGE(8, 0x76),     // Vendor Defined
        HID_RI_LOGICAL_MINIMUM(8, 0x00),
        HID_RI_LOGICAL_MAXIMUM(8, 0xFF),
        HID_RI_REPORT_COUNT(8, CONSOLE_EPSIZE),
        HID_RI_REPORT_SIZE(8, 0x08),
        HID_RI_OUTPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE | HID_IOF_NON_VOLATILE),
    HID_RI_END_COLLECTION(0)
};

inline constexpr uint8_t JoystickReport[] = {
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

#if 0
inline constexpr uint8_t report_desc_con[] = {
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

inline constexpr uint8_t report_desc_raw[] = {
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

inline constexpr uint8_t CtapReport[] = {
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
