#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "keymap_thread.hpp"    // for signal_timeout()
#include "timer.hpp"



namespace key {

void timer_t::start_timer(pmap_t* slot)
{
    m_slot = slot;
    m_timeout_expected = true;
    arg = this;
    xtimer_set(this, m_timeout_us);
}

void timer_t::stop_timer()
{
    xtimer_remove(this);
    m_timeout_expected = false;
}

bool timer_t::timeout_expected()
{
    const bool timeout_expected = m_timeout_expected;
    m_timeout_expected = false;
    return timeout_expected;
}

void timer_t::handle_timeout(void* arg)
{
    timer_t* const that = static_cast<timer_t*>(arg);

    if ( that->timeout_expected() )
        // Timeout event is not deferred but handled immediately.
        // The slot is not used by on_timeout() itself but will be needed to call
        // .press/release() from on_timeout().
        that->on_timeout(that->m_slot);
    else
        DEBUG("Keymap:\e[0;34m spurious timeout (slot=%p)\e[0m\n", that->m_slot);
}

void timer_t::_tmo_key_timer(void* arg)
{
    keymap_thread::obj().signal_timeout(static_cast<timer_t*>(arg));
}

}  // namespace key