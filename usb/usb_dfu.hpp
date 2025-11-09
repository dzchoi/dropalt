#pragma once

#include "usb/dfu.h"
#include "riotboot/flashwrite.h"

#ifdef __cplusplus
extern "C" {
#endif

// USBUS DFU device interface context
typedef struct usbus_dfu_device {
    usbus_handler_t handler_ctrl;           // Control interface handler
    usbus_interface_t iface;                // Control interface
    usbus_descr_gen_t dfu_descr;            // DFU descriptor generator
    usbus_string_t slot0_str;               // Descriptor string for Slot 0
#if NUM_SLOTS == 2
    usbus_interface_alt_t iface_alt_slot1;  // Alt interface for secondary slot
    usbus_string_t slot1_str;               // Descriptor string for Slot 1
#endif
    riotboot_flashwrite_t writer;           // DFU firmware update state structure
    usbus_t* usbus;                         // Ptr to the USBUS context
    usb_dfu_state_t dfu_state;              // Internal DFU state machine
    uint8_t mode;                           // 0 - APP mode, 1 DFU mode
    int8_t selected_slot;                   // Slot used for upgrade
    int8_t skip_signature;                  // Skip RIOTBOOT signature status
} usbus_dfu_device_t;

// DFU initialization function
void usbus_dfu_init(usbus_t* usbus, usbus_dfu_device_t* handler);

// Minimum time, in milliseconds, that the host should wait before sending a subsequent
// DFU_GETSTATUS request.
constexpr uint32_t bwPollTimeout = 10;

// For Control Transfers with no Data stage (like DFU_DETACH), the Status stage must
// complete within 50 ms of the Setup stage. To ensure the host receives the ACK after
// DFU_DETACH, the reset must be delayed by at least 50 ms.
// Note that this is different from wDetachTimeOut (i.e. USB_DFU_DETACH_TIMEOUT_MS),
// which is used when USB_DFU_WILL_DETACH is NOT advertised.
constexpr uint32_t DFU_RESET_DELAY_MS = 50;

#ifdef __cplusplus
}
#endif
