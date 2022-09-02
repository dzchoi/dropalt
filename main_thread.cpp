#include "periph/wdt.h"
#include "xtimer.h"

int main()
{
    while ( true ) {
        wdt_kick();
        xtimer_sleep(1);
        LED0_TOGGLE;
    }

    return 0;
}
