// A variant of riotboot_dfu that boots from slot 0 and supports USB device firmware
// upgrade (DFU) through the USB2422 hub.

#include "board.h"              // for LED0_ON
#include "cpu.h"                // for RSTC
#include "panic.h"              // for core_panic_t
#include "riotboot/bootloader_selection.h"  // for BTN_BOOTLOADER_PIN
#include "riotboot/magic.h"     // for RIOTBOOT_MAGIC_ADDR, RIOTBOOT_MAGIC_NUMBER
#include "riotboot/slot.h"      // for riotboot_slot_jump(), ...
#include "thread.h"             // for thread_create(), sched_task_exit(), ...
#include "usb/usbus.h"          // for usbus_init(), usbus_create(), ...
#include "usb/usbus/dfu.h"      // for usbus_dfu_init(), ...
#include "usb2422.h"            // for usbhub_*()
#include "ztimer.h"             // for ztimer_init(), ztimer_periodic_wakeup(), ...

#ifdef BTN_BOOTLOADER_PIN
#include "periph/gpio.h"
#endif



#define DFU_FORCED  RIOTBOOT_MAGIC_NUMBER  // "RIOT"
#define DFU_ACTIVE  0x55464449  // "IDFU"

static bool stay_in_dfu(void)
{
    const uint32_t magic_number = *(uint32_t*)RIOTBOOT_MAGIC_ADDR;

    // If magic word was set to DFU_FORCED at the end of RAM, stay in DFU mode.
    if ( magic_number == DFU_FORCED )
        return true;

    // External reset (pressing Reset pin) while in DFU mode will not re-enter DFU mode.
    if ( magic_number == DFU_ACTIVE )
        return false;

#if defined(BTN_BOOTLOADER_PIN) && defined(BTN_BOOTLOADER_MODE)
    bool state;

    gpio_init(BTN_BOOTLOADER_PIN, BTN_BOOTLOADER_MODE);
    state = gpio_read(BTN_BOOTLOADER_PIN);
    /* If button configures w/ internal or external pullup, then it is an
    active-low, thus reverts the logic */
    if (BTN_BOOTLOADER_EXT_PULLUP || BTN_BOOTLOADER_MODE == GPIO_IN_PU ||
        BTN_BOOTLOADER_MODE == GPIO_OD_PU ) {
        return !state;
    } else {
        return state;
    }
#else
    // Resets other than power reset and system reset (e.g. external reset or watchdog
    // reset) will stay in DFU mode.
    return (RSTC->RCAUSE.bit.POR == 0) && (RSTC->RCAUSE.bit.SYST == 0);
#endif
}

// Create the usbus thread for DFU, replacing riotboot_usb_dfu_init() in
// riot/sys/riotboot/usb_dfu.c.
static void riotboot_usb_init(void)
{
    static usbus_t usbus;
    static usbus_dfu_device_t dfu;
    static char _usbus_stack[USBUS_STACKSIZE];

    usbus_init(&usbus, usbdev_get_ctx(0));
    usbus_dfu_init(&usbus, &dfu, USB_DFU_PROTOCOL_DFU_MODE);
    usbus_create(_usbus_stack, USBUS_STACKSIZE, USBUS_PRIO, USBUS_TNAME, &usbus);
}

static void wait_for_host_connection(uint8_t starting_port)
{
    ztimer_acquire(ZTIMER_MSEC);

    uint32_t now_ms = ztimer_now(ZTIMER_MSEC);
    uint32_t then_ms = now_ms;

    // Start with starting_port.
    usbhub_set_host_port(starting_port);

    while ( !usbhub_is_active() ) {
        // Switch to the other port after 1 sec.
        if ( (now_ms - then_ms) >= 1000u ) {
            then_ms = now_ms;
            usbhub_set_host_port(usbhub_extra_port());
        }
        // Take a catnap for 10 ms.
        ztimer_periodic_wakeup(ZTIMER_MSEC, &now_ms, 10);
    }

    ztimer_release(ZTIMER_MSEC);
}

// The "_main" thread is a temporary thread that runs until the USB2422 hub connects to
// a host through either of its ports. Then the "usbus" thread handles everything.
NORETURN static void* _main(void* arg)
{
    (void)arg;

    // Initialize ztimer before starting DFU mode.
    ztimer_init();

    // Create and run the USB thread.
    riotboot_usb_init();

    // Set up the USB2422 in the background.
    usbhub_init();
    wait_for_host_connection(USB_PORT_1);

    // Indicate that DFU mode is ready.
    LED0_ON;

    // Remove this thread from scheduler.
    sched_task_exit();
}

NORETURN void kernel_init(void)
{
    uint32_t* const magic_addr = (uint32_t*)RIOTBOOT_MAGIC_ADDR;

    if ( !stay_in_dfu() ) {
        const unsigned SLOT0 = 0;
        if ( (riotboot_slot_validate(SLOT0) == 0)
          && (riotboot_slot_get_hdr(SLOT0)->start_addr ==
                riotboot_slot_get_image_startaddr(SLOT0)) ) {
            *magic_addr = 0u;
            // Calling riotboot_slot_jump() from within a thread context can lead to a
            // crash, particularly when reset_handler_default() initializes static memory
            // that may overlap with the memory region allocated for a bootloader thread
            // (e.g., main_stack[]).
            riotboot_slot_jump(SLOT0);
        }
    }

    // Enter DFU mode from now on!
    *magic_addr = DFU_ACTIVE;
    irq_disable();

    static char _main_stack[THREAD_STACKSIZE_MAIN];
    thread_create(_main_stack, sizeof(_main_stack),
                  THREAD_PRIORITY_MAIN,
                  THREAD_CREATE_WOUT_YIELD,
                  _main, NULL, "_main");

    // Call context switching upon exit.
    cpu_switch_context_exit();
}

NORETURN void core_panic(core_panic_t crash_code, const char* message)
{
    (void)crash_code;
    (void)message;
    while (1) {}
}
