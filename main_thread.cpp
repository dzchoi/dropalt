#include "board.h"
#include "periph/wdt.h"
#include "ztimer.h"



int main()
{
    while ( true ) {
        wdt_kick();
        ztimer_sleep(ZTIMER_MSEC, 1000);
        LED0_TOGGLE;
    }

    return 0;
}
