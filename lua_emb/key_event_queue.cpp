#include "assert.h"
#include "log.h"
#include "mutex.h"              // for mutex_lock(), mutex_unlock(), ...
#include "periph_conf.h"        // for NUM_MATRIX_SLOTS
#include "ztimer.h"             // for ztimer_mutex_lock_timeout()

#include "key_event_queue.hpp"
#include "main_thread.hpp"      // for main_thread::is_active()



namespace key {

mutex_t event_queue::m_access_lock = MUTEX_INIT;
mutex_t event_queue::m_full_lock = MUTEX_INIT;

event_queue::entry_t event_queue::m_buffer[QUEUE_SIZE];

size_t event_queue::m_push = 0;
size_t event_queue::m_peek = 0;
size_t event_queue::m_pop = 0;

defer_t* event_queue::m_deferrer = nullptr;

bool (*event_queue::m_next_event)(entry_t*) = &event_queue::try_pop;



bool event_queue::push(unsigned slot_index, bool is_press, uint32_t timeout_us)
{
    assert( slot_index < NUM_MATRIX_SLOTS );

    // Wait until the queue is not full.
    if ( timeout_us == 0 )
        mutex_lock(&m_full_lock);
    else if ( ztimer_mutex_lock_timeout(ZTIMER_USEC, &m_full_lock, timeout_us) != 0 )
        return false;

    mutex_lock(&m_access_lock);

    // Lazy cleanup of the queue.
    if ( m_push == QUEUE_SIZE ) {
        assert( m_pop > 0 );  // Because the queue is no longer full.
        __builtin_memmove(&m_buffer[0], &m_buffer[m_pop],
            (QUEUE_SIZE - m_pop) * sizeof(entry_t));
        m_push -= m_pop;
        m_peek -= m_pop;
        m_pop = 0;
    }

    // Push the new event.
    m_buffer[m_push++] = {{ uint8_t(slot_index), is_press }};

    mutex_unlock(&m_access_lock);

    // Unlok m_full_lock if the queue is not full.
    if ( m_push < QUEUE_SIZE )
        mutex_unlock(&m_full_lock);

    return true;
}

bool event_queue::try_pop(entry_t* pevent)
{
    mutex_lock(&m_access_lock);

    if ( m_pop == m_push ) {
        mutex_unlock(&m_access_lock);
        return false;
    }

    if ( pevent ) {
        *pevent = m_buffer[m_pop++];
        // Here reset the peek point!
        m_peek = m_pop;
    }

    mutex_unlock(&m_access_lock);
    mutex_unlock(&m_full_lock);  // No harm if m_full_lock was not locked.
    return true;
}

bool event_queue::try_peek(entry_t* pevent)
{
    mutex_lock(&m_access_lock);

    if ( m_peek == m_push ) {
        mutex_unlock(&m_access_lock);
        return false;
    }

    if ( pevent )
        *pevent = m_buffer[m_peek++];

    mutex_unlock(&m_access_lock);
    return true;
}

void event_queue::start_defer(defer_t* pmap)
{
    LOG_DEBUG("Keymap: start defer\n");
    assert( m_deferrer == nullptr );
    assert( main_thread::is_active() );

    m_deferrer = pmap;
    m_next_event = &event_queue::try_peek;
}

void event_queue::stop_defer()
{
    LOG_DEBUG("Keymap: stop defer\n");
    assert( m_deferrer != nullptr );
    assert( main_thread::is_active() );

    m_deferrer = nullptr;
    m_next_event = &event_queue::try_pop;
}

bool event_queue::is_deferred(unsigned slot_index, bool is_press)
{
    const entry_t event = {{ uint8_t(slot_index), is_press }};
    mutex_lock(&m_access_lock);

    for ( size_t i = m_pop ; i < m_peek ; i++ )
        if ( m_buffer[i].value == event.value ) {
            mutex_unlock(&m_access_lock);
            return true;
        }

    mutex_unlock(&m_access_lock);
    return false;
}

void event_queue::remove_last_deferred()
{
    mutex_lock(&m_access_lock);

    if ( m_pop == m_peek ) {
        mutex_unlock(&m_access_lock);
        return;
    }

    m_pop++;
    __builtin_memmove(&m_buffer[m_pop], &m_buffer[m_pop - 1],
        (m_peek - m_pop) * sizeof(entry_t));

    mutex_unlock(&m_access_lock);
    mutex_unlock(&m_full_lock);  // No harm if m_full_lock was not locked.
}

}
