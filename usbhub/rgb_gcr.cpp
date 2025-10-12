#include "is31fl3733.h"         // for is31_set_gcr(), is31_set_ssd_lock()

#include "adc.hpp"              // for adc_v_5v::lock_guard, adc::v_5v.level()
#include "log.h"
#include "rgb_gcr.hpp"
#include "usbhub_thread.hpp"    // for usbhub_thread::signal_event()



void rgb_gcr::disable()
{
    if ( !m_enabled )
        return;

    adc::lock_guard gcr_lock;
    m_enabled = false;
    is31_set_gcr(m_current_gcr = 0);
    is31_set_ssd_lock(true);
}

void rgb_gcr::isr_process_v_5v_report()
{
    if ( !m_enabled )
        return;

    // Lower GCR by 1 if necessary.
    if ( m_current_gcr > m_desired_gcr || adc::v_5v.level() < adc_v_5v::MID ) {
        static event_t _event = { nullptr,  // .list_node
            [](event_t*) {  // .handler
                // This check is still necessary, even though isr_process_v_5v_report()
                // also checks it. For example, FLAG_USB_SUSPEND may be immediately
                // followed by FLAG_LOWER_GCR, resulting in both signals being processed
                // in sequence.
                if ( !m_enabled )
                    return;

                adc::lock_guard gcr_lock;
                is31_set_gcr(--m_current_gcr);
                // Lock SSD if the updated GCR is 0.
                if ( m_current_gcr == 0 )
                    is31_set_ssd_lock(true);
                // LOG_DEBUG("Rgb: v_5v=%d v_con1=%d v_con2=%d gcr=%d",
                //     adc::v_5v.read(), adc::v_con1.read(), adc::v_con2.read(),
                //     m_current_gcr);
            }
        };

        usbhub_thread::signal_event(&_event);
    }

    // Raise GCR by 1 if necessary.
    else if ( m_current_gcr < m_desired_gcr ) {
        static event_t _event = { nullptr,  // .list_node
            [](event_t*) {  // .handler
                if ( !m_enabled )
                    return;

                adc::lock_guard gcr_lock;
                // Release SSD if it is locked.
                if ( m_current_gcr == 0 )
                    is31_set_ssd_lock(false);
                is31_set_gcr(++m_current_gcr);
                // LOG_DEBUG("Rgb: v_5v=%d v_con1=%d v_con2=%d gcr=%d",
                //     adc::v_5v.read(), adc::v_con1.read(), adc::v_con2.read(),
                //     m_current_gcr);
            }
        };

        usbhub_thread::signal_event(&_event);
    }
}
