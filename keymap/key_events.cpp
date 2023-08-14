#include "log.h"

#include "assert.h"             // for assert()
#include "mutex.h"              // for mutex_lock(), mutex_unlock()
#include "periph_conf.h"        // for NUM_SLOTS
#include "ztimer.h"             // for ZTIMER_USEC

#include "cond_timed_wait.hpp"  // for cond_wait_for(), cond_wait(), cond_signal()
#include "key_events.hpp"
#include "keymap_thread.hpp"    // for is_active()



namespace key {

events_t::events_t()
{
    cond_init(&m_not_full);
    mutex_init(&m_lock);
}

bool events_t::push(size_t index, bool is_press, uint32_t timeout_us)
{
    assert( index < NUM_SLOTS );
    mutex_lock(&m_lock);

    // Wait until the buffer is not full.
    while ( (m_push - m_pop) == BUFFER_SIZE ) {  // due to Mesa-style condition variables
        if ( timeout_us == 0u )
            cond_wait(&m_not_full, &m_lock);
        else if ( !cond_timed_wait(ZTIMER_USEC, &m_not_full, &m_lock, timeout_us) )
            // Todo: If spurious wake-up occurs we need to compensate it for next
            //  timeout_us.
            return false;
    }

    // Clean up the buffer lazily.
    if ( m_push == BUFFER_SIZE ) {
        assert( m_pop > 0 );
        __builtin_memmove(&m_buffer[0], &m_buffer[m_pop],
            (BUFFER_SIZE - m_pop) * sizeof(key_event_t));
        m_push -= m_pop;
        m_peek -= m_pop;
        m_pop = 0;
    }

    // Push the new event.
    m_buffer[m_push++] = {{ uint8_t(index), is_press }};

    mutex_unlock(&m_lock);
    return true;
}

bool events_t::try_pop(key_event_t* pevent)
{
    mutex_lock(&m_lock);

    if ( m_pop == m_push ) {
        mutex_unlock(&m_lock);
        return false;
    }

    if ( pevent ) {
        *pevent = m_buffer[m_pop++];
        // Reset the peek point.
        m_peek = m_pop;
        cond_signal(&m_not_full);
    }

    mutex_unlock(&m_lock);
    return true;
}

bool events_t::try_peek(key_event_t* pevent)
{
    mutex_lock(&m_lock);

    if ( m_peek == m_push ) {
        mutex_unlock(&m_lock);
        return false;
    }

    if ( pevent )
        *pevent = m_buffer[m_peek++];

    mutex_unlock(&m_lock);
    return true;
}

bool events_t::is_deferred(size_t index, bool is_press) const
{
    const key_event_t event = {{ uint8_t(index), is_press }};
    mutex_lock(&m_lock);

    for ( size_t i = m_pop ; i < m_peek ; ++i )
        if ( m_buffer[i].data == event.data ) {
            mutex_unlock(&m_lock);
            return true;
        }

    mutex_unlock(&m_lock);
    return false;
}

void events_t::discard_last_deferred()
{
    mutex_lock(&m_lock);

    if ( m_pop < m_peek ) {
        m_pop++;
        __builtin_memmove(&m_buffer[m_pop], &m_buffer[m_pop - 1],
            (m_peek - m_pop) * sizeof(key_event_t));
        cond_signal(&m_not_full);
    }

    mutex_unlock(&m_lock);
}

void events_t::start_defer(defer_t* pmap)
{
    LOG_DEBUG("Keymap: start defer\n");
    assert( m_deferrer == nullptr );
    assert( keymap_thread::obj().is_active() );
    m_deferrer = pmap;
    m_next_event = &events_t::try_peek;
}

void events_t::stop_defer()
{
    LOG_DEBUG("Keymap: stop defer\n");
    assert( m_deferrer != nullptr );
    assert( keymap_thread::obj().is_active() );
    m_deferrer = nullptr;
    m_next_event = &events_t::try_pop;
}

}  // namespace key
