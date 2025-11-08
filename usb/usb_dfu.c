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

// Represents wTransferSize: the maximum number of bytes the device can accept per
// Control transfer. This value directly impacts download and upload speeds. Any positive
// number is valid and does not require additional memory allocation.
#define DEFAULT_XFER_SIZE 1024



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
    if_desc.attribute = USB_DFU_WILL_DETACH | USB_DFU_MANIFEST_TOLERANT
                      | USB_DFU_CAN_DOWNLOAD | USB_DFU_CAN_UPLOAD;
#else
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
    .fmt_pre_descriptor = NULL,
    .fmt_post_descriptor = _gen_dfu_descriptor,
    .len = {
        .fixed_len = sizeof(usb_desc_if_dfu_t),
    },
    .len_type = USBUS_DESCR_LEN_FIXED,
};

void usbus_dfu_init(usbus_t* usbus, usbus_dfu_device_t* dfu)
{
    DEBUG("DFU: initialization");
    static_assert( FLASHPAGE_SIZE > 0 );
    static_assert( (SLOT0_OFFSET % FLASHPAGE_SIZE) == 0,
                   "SLOT0_OFFSET has to be a multiple of FLASHPAGE_SIZE" );

    __builtin_memset(dfu, 0, sizeof(usbus_dfu_device_t));
    dfu->usbus = usbus;
    dfu->handler_ctrl.driver = &dfu_driver;
#ifdef RIOTBOOT
    dfu->mode = USB_DFU_PROTOCOL_DFU_MODE;
    dfu->dfu_state = USB_DFU_STATE_DFU_IDLE;
#else
    dfu->mode = USB_DFU_PROTOCOL_RUNTIME_MODE;
    dfu->dfu_state = USB_DFU_STATE_APP_IDLE;
#endif
    dfu->selected_slot = -1;   // undetermined
    dfu->skip_signature = -1;  // undetermined

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

    // Since USB_DFU_WILL_DETACH is advertised, DFU_DETACH is used to trigger a reboot
    // instead of using a USB reset event.
    // usbus_handler_set_flag(handler, USBUS_HANDLER_FLAG_RESET);
}

static void _tmo_detach(void* arg)
{
    (void)arg;
#ifdef RIOTBOOT
    // Always trigger a system reset from the bootloader. If Lua bytecode is unavailable,
    // an assert failure will fall back to the bootloader.
    // Note: using DFU_DETACH in DFU mode is not compliant with the DFU 1.1 standard, but
    // `dfu-util -R` does send it anyway. See
    // https://github.com/Stefan-Schmidt/dfu-util/blob/master/src/main.c#L1159.
    system_reset();
#else
    enter_bootloader();
#endif
}

// According to _recv_setup() in riot/sys/usb/usbus/usbus_control.c, _control_handler()
// should return 1 on success or -1 on error. Each request sub-handler should also follow
// the same convention.

static int dfu_detach_handler(usbus_t* usbus, usbus_dfu_device_t* dfu, usb_setup_t* pkt)
{
    (void)usbus;
    (void)dfu;
    (void)pkt;

#ifndef RIOTBOOT
    dfu->dfu_state = USB_DFU_STATE_APP_DETACH;
#endif

    static ztimer_t timer = { .callback = &_tmo_detach, .arg = NULL };
    ztimer_set(ZTIMER_MSEC, &timer, DFU_RESET_DELAY_MS);
    return 1;
}

#ifdef RIOTBOOT
void matrix_enable(void);
void matrix_disable(void);

static int dfu_dnload_handler(usbus_t* usbus, usbus_dfu_device_t* dfu, usb_setup_t* pkt)
{
    // If the host indicates the end of the download, we finalize the flash and
    // transition to DFU_MANIFEST_SYNC.
    if ( pkt->length == 0 ) {
        riotboot_flashwrite_flush(&dfu->writer);
        // skip_signature should be either 0 or 1 here.
        if ( dfu->skip_signature )
            riotboot_flashwrite_finish(&dfu->writer);

        DEBUG("DFU: DFU_DNLOAD end (%d bytes)", dfu->writer.offset);
        dfu->dfu_state = USB_DFU_STATE_DFU_MANIFEST_SYNC;
        return 1;
    }

    switch ( dfu->dfu_state ) {
    // When receiving the request at Setup stage we transition the state from DFU_IDLE/
    // DFU_DL_IDLE to DFU_DL_SYNC. No data packet has been received yet.

        case USB_DFU_STATE_DFU_IDLE:
            DEBUG("DFU: DFU_DNLOAD start");
            // Disable matrix interrupt during firmware flashing.
            matrix_disable();
            dfu->skip_signature = -1;
            // Intentional fall-through

        case USB_DFU_STATE_DFU_DL_IDLE:
            dfu->dfu_state = USB_DFU_STATE_DFU_DL_SYNC;
            break;

    // During the Data stage, we process each 64-byte packet of DFU download data from
    // the host.

        case USB_DFU_STATE_DFU_DL_SYNC: {
            size_t data_size = 0;
            const uint8_t* data = usbus_control_get_out_data(usbus, &data_size);

            // If skip_signature is not determined, it means we are at the very first
            // 64-byte packet of the first DFU_DNLOAD request.
            if ( unlikely(dfu->skip_signature == -1) ) {
                // Avoid underflow condition
                if ( data_size < RIOTBOOT_FLASHWRITE_SKIPLEN )
                    goto error;

                // Determine skip_signature by checking if the packet starts with a slot
                // header.
                dfu->skip_signature = (*(uint32_t*)(void*)data == RIOTBOOT_MAGIC);
                if ( dfu->skip_signature ) {
                    // Skip writing the magic number ("RIOT") in the slot header.
                    riotboot_flashwrite_init(&dfu->writer, dfu->selected_slot);
                    data_size -= RIOTBOOT_FLASHWRITE_SKIPLEN;
                    data += RIOTBOOT_FLASHWRITE_SKIPLEN;
                }
                else {
                    // Headerless firmware image.
                    riotboot_flashwrite_init_raw(
                        &dfu->writer, dfu->selected_slot, 0);
                }
            }

            int success = riotboot_flashwrite_putbytes(
                &dfu->writer, data, data_size, true);
            if ( success == 0 )
                break;
        }
        // Intentional fall-through

        default:
        error:
            // Error occurred, stall the current transfer.
            dfu->dfu_state = USB_DFU_STATE_DFU_ERROR;
            return -1;
    }

    return 1;
}

static int dfu_upload_handler(usbus_t* usbus, usbus_dfu_device_t* dfu, usb_setup_t* pkt)
{
    // Note: We read directly from backup RAM without stashing the data. The first
    // 64-byte packet is sent to the host reliably thanks to irq_disable(). However, if
    // the backup RAM is full and a new log entry larger than 64 bytes arrives between
    // the first and second packet transmissions, the second packet may be truncated,
    // and no further packets will be sent.
    unsigned irq = irq_disable();

    static const char* logs;
    static size_t read_offset;
    static int last_block;

    // Initialization when transitioning from DFU_IDLE to DFU_UP_IDLE.
    if ( dfu->dfu_state == USB_DFU_STATE_DFU_IDLE ) {
        DEBUG("DFU: DFU_UPLOAD start");
        dfu->dfu_state = USB_DFU_STATE_DFU_UP_IDLE;
        logs = backup_ram_read();
        read_offset = 0;
        last_block = -1;
    }

    static const uint8_t* data;
    static size_t data_size;

    // Initialization during each Setup stage. Subsequent calls during the Data stage
    // will have the same block number (pkt->value).
    if ( last_block != pkt->value ) {
        last_block = pkt->value;
        data = (const uint8_t*)&logs[read_offset];
        data_size = 0;
        // Note that pkt->length is the total data size requested from the host in a
        // transfer. It will be usually the same as wTransferSize.
        while ( data_size < pkt->length && data[data_size] )
            data_size++;
    }

    // Note that usbus_control_slicer_put_bytes() must be called with the same buffer and
    // buffer size throughout the transfer (Setup stage and Data stage).
    size_t packet_size = usbus_control_slicer_put_bytes(usbus, data, data_size);
    // If the last packet's size is a multiple of the endpoint (EP) size, packet_size can
    // be 0 subsequently.
    read_offset += packet_size;

    irq_restore(irq);

    // A short packet (smaller than the EP size) completes the transfer.
    if ( packet_size < ((usbus_control_handler_t *)usbus->control)->in->len ) {
        DEBUG("DFU: DFU_UPLOAD end (%d bytes)", read_offset);
        dfu->dfu_state = USB_DFU_STATE_DFU_IDLE;
    }

    return 1;
}

static int dfu_clrstatus_handler(usbus_t* usbus, usbus_dfu_device_t* dfu, usb_setup_t* pkt)
{
    (void)usbus;
    (void)pkt;

    if ( dfu->dfu_state == USB_DFU_STATE_DFU_ERROR )
        dfu->dfu_state = USB_DFU_STATE_DFU_IDLE;
    else
        DEBUG("CLRSTATUS: unhandled case");

    return 1;
}

static int dfu_abort_handler(usbus_t* usbus, usbus_dfu_device_t* dfu, usb_setup_t* pkt)
{
    (void)usbus;
    (void)pkt;

    matrix_enable();
    dfu->dfu_state = USB_DFU_STATE_DFU_IDLE;
    return 1;
}
#endif

static int dfu_getstatus_handler(usbus_t* usbus, usbus_dfu_device_t* dfu, usb_setup_t* pkt)
{
    (void)pkt;

#ifdef RIOTBOOT
    switch ( dfu->dfu_state ) {
        case USB_DFU_STATE_DFU_DL_SYNC:
            // Flashing of a block is complete, since packets are written as they are
            // received.
            dfu->dfu_state = USB_DFU_STATE_DFU_DL_IDLE;
            break;

        case USB_DFU_STATE_DFU_MANIFEST_SYNC:
            // Download is finished. As a manifestation tolerant device, we do not
            // reboot, but stay in DFU mode.
            matrix_enable();
            dfu->dfu_state = USB_DFU_STATE_DFU_IDLE;
            break;

        default:
            break;
    }
#endif

    // Send the response back to the host.
    dfu_get_status_pkt_t buf = {
        .status = 0,
        .timeout = bwPollTimeout,
        .state = dfu->dfu_state,
        .string = 0
    };
    usbus_control_slicer_put_bytes(usbus, (uint8_t*)&buf, sizeof(buf));
    DEBUG("DFU: respond to DFU_GETSTATUS (state=%d)", dfu->dfu_state);

    return 1;
}

static int _control_handler(usbus_t* usbus, usbus_handler_t* handler,
    usbus_control_request_state_t state, usb_setup_t* setup)
{
    (void)state;

    usbus_dfu_device_t* dfu = container_of(handler, usbus_dfu_device_t, handler_ctrl);
    DEBUG("DFU: Control request: req_state=0x%x block=%d request=0x%x",
        state, setup->value, setup->request);

    // Process DFU Class requests.
    if ( setup->type & USB_SETUP_REQUEST_TYPE_CLASS ) {
        switch ( setup->request ) {
            case DFU_DETACH:
                return dfu_detach_handler(usbus, dfu, setup);

#ifdef RIOTBOOT
            case DFU_DOWNLOAD:
                return dfu_dnload_handler(usbus, dfu, setup);

            case DFU_UPLOAD:
                return dfu_upload_handler(usbus, dfu, setup);

            case DFU_CLR_STATUS:
                return dfu_clrstatus_handler(usbus, dfu, setup);

            case DFU_ABORT:
                return dfu_abort_handler(usbus, dfu, setup);
#endif

            case DFU_GET_STATUS:
                return dfu_getstatus_handler(usbus, dfu, setup);

            default:
                DEBUG("Unhandled DFU control request:%d", setup->request);
                // return -1;
        }
    }

    // Process Standard requests.
    else {
        if ( setup->request == USB_SETUP_REQ_SET_INTERFACE ) {
            DEBUG("DFU: Select alt interface %d", setup->value);
            dfu->selected_slot = setup->value;
            return 1;
        }
    }

    return -1;
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
    (void)event;
}
