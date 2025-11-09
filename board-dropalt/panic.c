// Custom panic.c that replaces riot/core/lib/panic.c

#include "board.h"              // for enter_dfu_mode()
#include "irq.h"
#include "log.h"
#include "panic.h"
#include "periph/pm.h"

#if defined(DEVELHELP) && defined(MODULE_PS)
#include "ps.h"
#endif



void __attribute__((weak)) panic_arch(void) {}

NORETURN void core_panic(core_panic_t crash_code, const char* message)
{
    (void)crash_code;

    // flag preventing "recursive crash printing loop"
    static int crashed = 0;
    if ( crashed == 0 ) {
        crashed = 1;
        LOG_ERROR("*** RIOT kernel panic: %s", message);
#if defined(DEVELHELP) && defined(MODULE_PS)
        ps();
#endif
    }

    // Disable all maskable interrupts.
    irq_disable();
    panic_arch();

    LOG_ERROR("*** rebooting...");
    enter_dfu_mode();
}
