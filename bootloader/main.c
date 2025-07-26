// A variant of riotboot_dfu that boots from slot 0 and supports USB device firmware
// upgrade (DFU) through the USB2422 hub.

#include "board.h"              // for LED0_ON, sam0_flashpage_aux_cfg(), ...
#include "compiler_hints.h"     // for UNREACHABLE()
#include "cpu.h"                // for RSTC
#include "panic.h"              // for core_panic_t
#include "periph/gpio.h"        // for gpio_init(), gpio_init_int(), ...
// #include "riotboot/bootloader_selection.h"  // for BTN_BOOTLOADER_PIN
#include "riotboot/magic.h"     // for RIOTBOOT_MAGIC_ADDR, RIOTBOOT_MAGIC_NUMBER
#include "riotboot/slot.h"      // for riotboot_slot_jump(), ...
#include "seeprom.h"            // for seeprom_init(), seeprom_sync(), ...
#include "sr_exp.h"             // for sr_exp_init()
#include "thread.h"             // for thread_create(), sched_task_exit(), ...
#include "usb/usbus.h"          // for usbus_init(), usbus_create(), ...
#include "usb_dfu.h"            // for usbus_dfu_init(), ...
#include "usb2422.h"            // for usbhub_*()
#include "ztimer.h"             // for ztimer_init(), ztimer_periodic_wakeup(), ...



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

static uint8_t nvm_init(void)
{
    nvm_user_page_t user_page = *sam0_flashpage_aux_cfg();

    // Set up SEEPROM if not set up yet. Changes will take effect after reset.
    if ( user_page.smart_eeprom_blocks != SEEPROM_SBLK
      || user_page.smart_eeprom_page_size != SEEPROM_PSZ ) {
        user_page.smart_eeprom_blocks = SEEPROM_SBLK;
        user_page.smart_eeprom_page_size = SEEPROM_PSZ;
        char* s = (char*)sam0_flashpage_aux_get(0);
        s = (*s == '\xff' ? NULL : __builtin_strdup(s));

        // Erase the USER page while preserving the reserved section (the first 32 bytes).
        sam0_flashpage_aux_reset(&user_page);

        // Restore the product serial.
        if ( s ) {
            sam0_flashpage_aux_write(0, s, __builtin_strlen(s) + 1);
            __builtin_free(s);
        }

        reboot_to_bootloader();
        UNREACHABLE();
    }

    seeprom_init();
    seeprom_sync();

    // Try to read `persistent::get("last_host_port", port)` directly from NVM, taking
    // advantage of its position as the first entry.
    if ( *(uint32_t*)seeprom_addr(0) == 0x73616c15u )  // 's', 'a', 'l', 0x15
        return *(uint8_t*)seeprom_addr(__builtin_strlen("last_host_port") + 2);

    // Clear NVM if it is not in right format.
    *(uint8_t*)seeprom_addr(0) = 0xff;
    return USB_PORT_1;
}

static void wait_for_host_connection(uint8_t port)
{
    ztimer_acquire(ZTIMER_MSEC);

    uint32_t now_ms = ztimer_now(ZTIMER_MSEC);
    uint32_t then_ms = now_ms;

    usbhub_select_host_port(port);

    while ( !usbhub_is_active() ) {
        // Switch to the other port after 1 sec.
        if ( (now_ms - then_ms) >= 1000u ) {
            then_ms = now_ms;
            usbhub_select_host_port(usbhub_extra_port());
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

    // Initialize Shift Register.
    sr_exp_init();

    // Initialize ztimer before starting DFU mode.
    ztimer_init();

    // Initialize NVM and read `last_host_port` if available.
    uint8_t starting_port = nvm_init();

    // Create and run the usbus thread.
    riotboot_usb_init();

    // Set up the USB2422 hub.
    usbhub_init();
    wait_for_host_connection(starting_port);

    // Enable matrix interrupt to catch a key press.
    void matrix_enable(void);
    matrix_enable();

    // Indicate that DFU mode is ready.
    LED0_ON;

    // Remove this thread from scheduler.
    sched_task_exit();
}



// Override the weak pre_startup() function in riot/cpu/cortexm_common/vectors_cortexm.c.
// This function is called before cpu_init(), board_init() and kernel_init(), and jumps
// immediately to the application in slot 0 if needed.
void pre_startup(void)
{
    static const unsigned SLOT0 = 0;
    static uint32_t* const MAGIC_ADDR = (uint32_t*)RIOTBOOT_MAGIC_ADDR;

    // If *MAGIC_ADDR equals RIOTBOOT_MAGIC_NUMBER, stay in DFU mode.
    if ( *MAGIC_ADDR == RIOTBOOT_MAGIC_NUMBER ) {
        *MAGIC_ADDR = 0u;
        return;
    }

    // If we are rebooting due to a system reset or power reset, jump to the application
    // in slot 0 if it is valid.
    if ( (RSTC->RCAUSE.bit.SYST != 0) || (RSTC->RCAUSE.bit.POR != 0) ) {
        // The functions called here are only memory operations on NVM region, hence
        // allowed at this early stage of booting. They do not execute log_write() since
        // LOG_LEVEL = LOG_NONE, and assert() is disabled as NDEBUG = 1.
        if ( riotboot_slot_validate(SLOT0) == 0 ) {
            riotboot_slot_jump(SLOT0);
            UNREACHABLE();
        }
    }

    // Any other resets, such as an external reset or watchdog reset, will stay in DFU
    // mode.
}

NORETURN void kernel_init(void)
{
    // Welcome to the DFU mode!
    irq_disable();

    static char _main_stack[THREAD_STACKSIZE_MAIN];
    thread_create(_main_stack, sizeof(_main_stack),
                  THREAD_PRIORITY_MAIN,
                  THREAD_CREATE_WOUT_YIELD,
                  _main, NULL, "_main");

    // Call context switching upon exit.
    cpu_switch_context_exit();
}

static const gpio_t col = col_pins[13];  // Matrix column that contains ENTER key.
static const gpio_t row = row_pins[2];   // Matrix row that contains ENTER key.
// For testing:
// static const gpio_t col = col_pins[14];  // RIGHT key.
// static const gpio_t row = row_pins[4];   // RIGHT key.

void matrix_disable(void)
{
    gpio_irq_disable(row);
}

NORETURN static void _isr_jump_to_application(void* arg)
{
    (void)arg;
    matrix_disable();
    system_reset();
}

void matrix_enable(void)
{
    gpio_init(col, GPIO_OUT);
    gpio_set(col);
    gpio_init_int(row, GPIO_IN_PD, GPIO_RISING, _isr_jump_to_application, NULL);
    gpio_irq_enable(row);
}

NORETURN void core_panic(core_panic_t crash_code, const char* message)
{
    (void)crash_code;
    (void)message;
    while (1) {}
}
