// Custom dfu.c that replaces riot/sys/usb/usbus/dfu/dfu.c

#define USB_H_USER_IS_RIOT_INTERNAL

#include "backup_ram.h"         // for backup_ram_read()
#include "board.h"              // for system_reset(), enter_bootloader()
#include "usb/dfu.h"
#include "usb/descriptor.h"
#include "usb/usbus.h"
#include "usb/usbus/control.h"
#include "usb_dfu.h"
#include "riotboot/slot.h"
#include "riotboot/usb_dfu.h"
#include "ztimer.h"

#define ENABLE_DEBUG    0
#include "debug.h"



static void _event_handler(usbus_t* usbus, usbus_handler_t* handler,
                          usbus_event_usb_t event);
static int _control_handler(usbus_t* usbus, usbus_handler_t* handler,
                            usbus_control_request_state_t state,
                            usb_setup_t* setup);
static void _transfer_handler(usbus_t* usbus, usbus_handler_t* handler,
                              usbdev_ep_t* ep, usbus_event_transfer_t event);
static void _init(usbus_t* usbus, usbus_handler_t* handler);

#define DEFAULT_XFER_SIZE 64



// The active DFU mode is determined by the build context:
//   - Bootloader: DFU mode (used for firmware updates)
//   - Firmware: DFU Runtime mode (used during normal operation)

static size_t _gen_dfu_descriptor(usbus_t* usbus, void* arg)
{
    (void)arg;
    usb_desc_if_dfu_t if_desc;

    // functional dfu descriptor
    if_desc.length = sizeof(usb_desc_if_dfu_t);
    if_desc.type = USB_IF_DESCRIPTOR_DFU;
#ifdef RIOTBOOT
    // Configures as manifestation tolerant device to not reboot after manifesting.
    if_desc.attribute = USB_DFU_MANIFEST_TOLERANT
                      | USB_DFU_CAN_DOWNLOAD | USB_DFU_CAN_UPLOAD;
#else
    // USB_DFU_WILL_DETACH applies only for DFU Runtime mode.
    if_desc.attribute = USB_DFU_WILL_DETACH;
#endif
    if_desc.detach_timeout = USB_DFU_DETACH_TIMEOUT_MS;
    if_desc.xfer_size = DEFAULT_XFER_SIZE;
    if_desc.bcd_dfu = USB_DFU_VERSION_BCD;

    usbus_control_slicer_put_bytes(usbus, (uint8_t*)&if_desc, sizeof(if_desc));
    return sizeof(usb_desc_if_dfu_t);
}

static const usbus_handler_driver_t dfu_driver = {
    .init = _init,
    .event_handler = _event_handler,
    .transfer_handler = _transfer_handler,
    .control_handler = _control_handler,
};

// Descriptors
static const usbus_descr_gen_funcs_t _dfu_descriptor = {
    .fmt_post_descriptor = _gen_dfu_descriptor,
    .fmt_pre_descriptor = NULL,
    .len = {
        .fixed_len = sizeof(usb_desc_if_dfu_t),
    },
    .len_type = USBUS_DESCR_LEN_FIXED,
};

// Note: DFU mode and DFU Runtime mode are compiled separately. Set `mode` according to
// the build context:
//   - USB_DFU_PROTOCOL_DFU_MODE when running in the bootloader
//   - USB_DFU_PROTOCOL_RUNTIME_MODE when running in firmware
void usbus_dfu_init(usbus_t* usbus, usbus_dfu_device_t* dfu, unsigned mode)
{
    DEBUG("DFU: initialization\n");
    // assert( usbus );
    // assert( dfu );
    static_assert( FLASHPAGE_SIZE > 0 );
    static_assert( (SLOT0_OFFSET % FLASHPAGE_SIZE) == 0,
                   "SLOT0_OFFSET has to be a multiple of FLASHPAGE_SIZE" );

    __builtin_memset(dfu, 0, sizeof(usbus_dfu_device_t));
    dfu->usbus = usbus;
    dfu->handler_ctrl.driver = &dfu_driver;
    dfu->mode = mode;
    dfu->selected_slot = -1u;
    dfu->skip_signature = true;
#ifdef RIOTBOOT
    dfu->dfu_state = USB_DFU_STATE_DFU_IDLE;
#else
    dfu->dfu_state = USB_DFU_STATE_APP_IDLE;
#endif

    usbus_register_event_handler(usbus, (usbus_handler_t*)dfu);
}

static void _init(usbus_t* usbus, usbus_handler_t* handler)
{
    usbus_dfu_device_t* dfu = container_of(handler, usbus_dfu_device_t, handler_ctrl);
    // Set up descriptor generators
    dfu->dfu_descr.next = NULL;
    dfu->dfu_descr.funcs = &_dfu_descriptor;
    dfu->dfu_descr.arg = dfu;

    // Configure Interface 0 as control interface
    dfu->iface.class = USB_DFU_INTERFACE;
    dfu->iface.subclass = USB_DFU_SUBCLASS_DFU;

    dfu->iface.protocol = dfu->mode;

    dfu->iface.descr_gen = &dfu->dfu_descr;
    dfu->iface.handler = handler;

    // Create needed string descriptor for the interface and its alternate settings
#ifdef RIOTBOOT
    usbus_add_string_descriptor(usbus, &dfu->slot0_str, USB_DFU_MODE_SLOT0_NAME);
#else
    usbus_add_string_descriptor(usbus, &dfu->slot0_str, USB_APP_MODE_SLOT_NAME);
#endif

    // Add string descriptor to the interface
    dfu->iface.descr = &dfu->slot0_str;

#if defined(RIOTBOOT) && NUM_SLOTS == 2
    // DFU Runtime mode does not allow multiple slots.
    // Create needed string descriptor for the alternate settings
    usbus_add_string_descriptor(usbus, &dfu->slot1_str, USB_DFU_MODE_SLOT1_NAME);

    // Add string descriptor to the alternate settings
    dfu->iface_alt_slot1.descr = &dfu->slot1_str;

    // attached alternate settings to their interface
    usbus_add_interface_alt(&dfu->iface, &dfu->iface_alt_slot1);
#endif

    // Add interface to the stack
    usbus_add_interface(usbus, &dfu->iface);
    usbus_handler_set_flag(handler, USBUS_HANDLER_FLAG_RESET);
}

static int dfu_class_control_req(usbus_t* usbus, usbus_dfu_device_t* dfu, usb_setup_t* pkt)
{
    DEBUG("DFU control request:%x\n", pkt->request);

    switch ( pkt->request ) {
#ifdef RIOTBOOT
        case DFU_DOWNLOAD:
            // Host indicates end of firmware download
            if ( pkt->length == 0 ) {
                // Set DFU to manifest sync
                dfu->dfu_state = USB_DFU_STATE_DFU_MANIFEST_SYNC;
                riotboot_flashwrite_flush(&dfu->writer);
                riotboot_flashwrite_finish(&dfu->writer);
            }

            else if ( dfu->dfu_state != USB_DFU_STATE_DFU_DL_SYNC ) {
                void matrix_disable(void);
                // Disable matrix interrupt during firmware flashing.
                matrix_disable();
                dfu->dfu_state = USB_DFU_STATE_DFU_DL_SYNC;
            }

            else {
                // Retrieve firmware data
                size_t len = 0;
                int ret = 0;
                uint8_t* data = usbus_control_get_out_data(usbus, &len);

                // Skip writing the magic number ("RIOT") at the slot header.
                if ( dfu->skip_signature ) {
                    // Avoid underflow condition
                    if ( len < RIOTBOOT_FLASHWRITE_SKIPLEN ) {
                        dfu->dfu_state = USB_DFU_STATE_DFU_ERROR;
                        return -1;
                    }
                    riotboot_flashwrite_init(&dfu->writer, dfu->selected_slot);
                    len -= RIOTBOOT_FLASHWRITE_SKIPLEN;
                    dfu->skip_signature = false;
                    ret = riotboot_flashwrite_putbytes(
                        &dfu->writer, &data[RIOTBOOT_FLASHWRITE_SKIPLEN], len, true);
                }
                else {
                    ret = riotboot_flashwrite_putbytes(&dfu->writer, data, len, true);
                }

                if ( ret < 0 ) {
                    // Error occurs, stall the current transfer
                    dfu->dfu_state = USB_DFU_STATE_DFU_ERROR;
                    return -1;
                }
            }
            break;

        case DFU_UPLOAD: {
            static const char* logs;
            static size_t read_offset;

            if ( dfu->dfu_state == USB_DFU_STATE_DFU_IDLE ) {
                dfu->dfu_state = USB_DFU_STATE_DFU_UP_IDLE;
                logs = backup_ram_read();
                read_offset = 0;
            }

            size_t read_size = 0;
            while ( read_size < pkt->length && logs[read_offset + read_size] )
                read_size++;

            // Report 0 bytes if necessary.
            read_offset += usbus_control_slicer_put_bytes(
                usbus, (const uint8_t*)&logs[read_offset], read_size);

            if ( read_size < pkt->length || pkt->length == 0 )
                dfu->dfu_state = USB_DFU_STATE_DFU_IDLE;
            break;
        }

        case DFU_GET_STATUS: {
            if ( dfu->dfu_state == USB_DFU_STATE_DFU_DL_SYNC ) {
                dfu->dfu_state = USB_DFU_STATE_DFU_DL_IDLE;
                DEBUG("GET STATUS GO TO IDLE\n");
            }
            else if ( dfu->dfu_state == USB_DFU_STATE_DFU_MANIFEST_SYNC ) {
                // Download is finished. As a manifestation tolerant device, we do not
                // reboot, but stay in DFU mode.
                dfu->skip_signature = true;
                dfu->dfu_state = USB_DFU_STATE_DFU_IDLE;
            }

            dfu_get_status_pkt_t buf = {
                .status = 0,
                .timeout = USB_DFU_DETACH_TIMEOUT_MS,
                .state = dfu->dfu_state,
                .string = 0
            };
            // Send answer to host
            usbus_control_slicer_put_bytes(usbus, (uint8_t*)&buf, sizeof(buf));
            DEBUG("send answer\n");
            break;
        }

        case DFU_CLR_STATUS:
            if ( dfu->dfu_state != USB_DFU_STATE_DFU_ERROR ) {
                DEBUG("CLRSTATUS: unhandled case");
                break;
            }
            // intentional fall-through

        case DFU_ABORT:
            dfu->dfu_state = USB_DFU_STATE_DFU_IDLE;
            break;
#else
        case DFU_DETACH: {
            const usbopt_enable_t disable = USBOPT_DISABLE;
            // Detach USB bus
            usbdev_set(usbus->dev, USBOPT_ATTACH, &disable, sizeof(usbopt_enable_t));
            // Restart and jump into the bootloader
            // Minor issue: It's better to reboot only after sending the ACK for this
            // request. Otherwise, dfu-util may warn "dfu-util: error detaching".
            enter_bootloader();
            break;
        }

        case DFU_GET_STATUS: {
            dfu_get_status_pkt_t buf = {
                .status = 0,
                .timeout = 0,
                .state = dfu->dfu_state,  // always USB_DFU_STATE_APP_IDLE
                .string = 0
            };
            // Send answer to host
            usbus_control_slicer_put_bytes(usbus, (uint8_t*)&buf, sizeof(buf));
            DEBUG("send answer\n");
            break;
        }

        case DFU_CLR_STATUS:
        case DFU_ABORT:
            break;
#endif

        default:
            DEBUG("Unhandled DFU control request:%d\n", pkt->request);
    }

    return 0;
}

static int _control_handler(usbus_t* usbus, usbus_handler_t* handler,
    usbus_control_request_state_t state, usb_setup_t* setup)
{
    (void)usbus;
    (void)state;

    usbus_dfu_device_t* dfu = container_of(handler, usbus_dfu_device_t, handler_ctrl);
    DEBUG("DFU: Request: 0x%x\n", setup->request);

    // Process DFU class request
    if ( setup->type & USB_SETUP_REQUEST_TYPE_CLASS ) {
        if ( dfu_class_control_req(usbus, dfu, setup) < 0 ) {
            DEBUG("DFU: control request %u failed\n", setup->request);
            return -1;
        }
    }

    else {
        switch ( setup->request ) {
            case USB_SETUP_REQ_SET_INTERFACE:
                DEBUG("DFU: Select alt interface %d\n", setup->value);
                dfu->selected_slot = (unsigned)setup->value;
                break;

            default:
                return -1;
        }
    }

    return 1;
}

static void _transfer_handler(usbus_t* usbus, usbus_handler_t* handler,
    usbdev_ep_t* ep, usbus_event_transfer_t event)
{
    (void)usbus;
    (void)handler;
    (void)ep;
    (void)event;
}

static void _event_handler(usbus_t* usbus, usbus_handler_t* handler,
    usbus_event_usb_t event)
{
    (void)usbus;
    (void)handler;

#ifdef RIOTBOOT
    usbus_dfu_device_t* dfu = container_of(handler, usbus_dfu_device_t, handler_ctrl);

    // To prevent rebooting during the initial USB connection, we check whether
    // `dfu->selected_slot != -1u`, which is true when `dfu-util -R` triggers a reset
    // after flashing.
    if ( event == USBUS_EVENT_USB_RESET && dfu->selected_slot != -1u ) {
        DEBUG("DFU: reset event @%lu", ztimer_now(ZTIMER_MSEC));
        system_reset();
    }
#endif

    DEBUG("Unhandled event :0x%x\n", event);
}
