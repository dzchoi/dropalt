#include "adc.hpp"              // for adc::v_5v, v_con1, v_con2
#include "board.h"              // for THREAD_PRIO_RGB
#include "config.hpp"           // for RGB_DISABLE_DURING_USB_SUSPEND, ...
#include "is31fl3733.h"         // for is31_init(), ...
#include "log.h"
#include "msg.h"                // for msg_init_queue()
#include "thread.h"             // for thread_create(), ...
#include "thread_flags.h"       // for thread_flags_wait_any(), thread_flags_set()

#include "rgb_thread.hpp"



thread_t* rgb_thread::m_pthread = nullptr;

#ifdef DEVELHELP
    char rgb_thread::m_thread_stack[THREAD_STACKSIZE_MEDIUM];
#else
    char rgb_thread::m_thread_stack[THREAD_STACKSIZE_SMALL];
#endif

static constexpr size_t QUEUE_SIZE = 16;  // must be a power of two.
static_assert( (QUEUE_SIZE > 0) && ((QUEUE_SIZE & (QUEUE_SIZE - 1)) == 0) );
msg_t rgb_thread::m_msg_queue[QUEUE_SIZE];

rgb_gcr rgb_thread::m_gcr;

void rgb_gcr::disable()
{
    if ( !m_enabled )
        return;

    m_enabled = false;
    is31_set_gcr(m_current_gcr = 0);
    is31_set_ssd_lock(true);
}

void rgb_gcr::process_v_5v_report()
{
    // This check is still necessary, even though signal_v_5v_report() also checks it.
    // For example, FLAG_USB_SUSPEND may be immediately followed by FLAG_REPORT_V_5V,
    // resulting in both signals being processed in sequence.
    if ( !m_enabled )
        return;

    // Lower GCR by 1 if necessary.
    if ( m_current_gcr > m_desired_gcr || adc::v_5v.level() < adc_v_5v::MID ) {
        is31_set_gcr(--m_current_gcr);
        // Lock SSD if the updated GCR is 0.
        if ( m_current_gcr == 0 )
            is31_set_ssd_lock(true);
        // LOG_DEBUG("Rgb: v_5v=%d v_con1=%d v_con2=%d gcr=%d\n",
        //     adc::v_5v.read(), adc::v_con1.read(), adc::v_con2.read(), m_current_gcr);
    }

    // Raise GCR by 1 if necessary.
    else if ( m_current_gcr < m_desired_gcr ) {
        // Release SSD if it is locked.
        if ( m_current_gcr == 0 )
            is31_set_ssd_lock(false);
        is31_set_gcr(++m_current_gcr);
        // LOG_DEBUG("Rgb: v_5v=%d v_con1=%d v_con2=%d gcr=%d\n",
        //     adc::v_5v.read(), adc::v_con1.read(), adc::v_con2.read(), m_current_gcr);
    }
}



void rgb_thread::init()
{
    m_pthread = thread_get_unchecked( thread_create(
        m_thread_stack, sizeof(m_thread_stack),
        THREAD_PRIO_RGB,
        THREAD_CREATE_STACKTEST,
        _thread_entry, nullptr, "rgb_thread") );
}

NORETURN void* rgb_thread::_thread_entry(void*)
{
    // msg_init_queue() should be called from the queue-owning thread.
    msg_init_queue(m_msg_queue, QUEUE_SIZE);

    is31_init();

    is31_enable_all_leds(true);

    // Enable GCR during initialization if RGB_DISABLE_DURING_USB_SUSPEND is false.
    if constexpr ( !RGB_DISABLE_DURING_USB_SUSPEND )
        m_gcr.enable();

    while ( true ) {
        thread_flags_t flags = thread_flags_wait_any(  // Zzz
            FLAG_MSG_EVENT
            | FLAG_USB_SUSPEND
            | FLAG_USB_RESUME
            | FLAG_REPORT_V_5V
            // | FLAG_SET_EFFECT
            );

        if constexpr ( RGB_DISABLE_DURING_USB_SUSPEND ) {
            if ( flags & FLAG_USB_SUSPEND )
                m_gcr.disable();

            if ( flags & FLAG_USB_RESUME )
                m_gcr.enable();
        }

        if ( flags & FLAG_REPORT_V_5V )
            m_gcr.process_v_5v_report();
    }
}
