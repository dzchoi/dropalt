#include "log.h"

#include "keymap_thread.hpp"    // for signal_event()
#include "timer.hpp"



namespace key {

void timer_t::start_timer(pmap_t* slot)
{
    m_timeout_expected = slot;
    arg = this;
    ztimer_set(ZTIMER_MSEC, this, m_timeout_ms);
}

void timer_t::stop_timer()
{
    ztimer_remove(ZTIMER_MSEC, this);
    m_timeout_expected = nullptr;
}

pmap_t* timer_t::timeout_expected()
{
    pmap_t* const slot = m_timeout_expected;
    m_timeout_expected = nullptr;
    return slot;
}

void timer_t::_hdlr_timeout(event_t* event)
{
    timer_t* const that = static_cast<event_ext_t<timer_t*>*>(event)->arg;
    pmap_t* const slot = that->timeout_expected();

    if ( slot )
        // Timeout event is not deferred but handled immediately.
        // The `slot` may not used by on_timeout() directly but will be needed for
        // on_timeout() to be able to call e.g. .press/release(slot).
        that->on_timeout(slot);
    else
        LOG_WARNING("Keymap: spurious timeout\n");
}

void timer_t::_tmo_key_timer(void* arg)
{
    timer_t* const that = static_cast<timer_t*>(arg);
    keymap_thread::obj().signal_event(&that->m_event_timeout);
}

}  // namespace key
