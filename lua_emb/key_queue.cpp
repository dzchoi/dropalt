#include "assert.h"
#include "log.h"
#include "mutex.h"              // for mutex_lock(), mutex_unlock(), ...
#include "periph_conf.h"        // for NUM_MATRIX_SLOTS
#include "ztimer.h"             // for ztimer_mutex_lock_timeout()

extern "C" {
// #include "lua.h"
#include "lauxlib.h"            // declares luaL_*().
}

#include "key_queue.hpp"
#include "main_thread.hpp"      // for main_thread::is_active()



mutex_t key_queue::m_access_lock = MUTEX_INIT;
mutex_t key_queue::m_full_lock = MUTEX_INIT;

static constexpr size_t QUEUE_SIZE = 16;  // must be a power of two.
static_assert( (QUEUE_SIZE > 0) && ((QUEUE_SIZE & (QUEUE_SIZE - 1)) == 0) );
key_queue::entry_t key_queue::m_buffer[QUEUE_SIZE];

size_t key_queue::m_push = 0;
size_t key_queue::m_peek = 0;
size_t key_queue::m_pop = 0;

int key_queue::m_rdeferrer = LUA_NOREF;

bool (*key_queue::m_get)(entry_t*) = &key_queue::try_pop;



bool key_queue::push(entry_t event, uint32_t timeout_us)
{
    // Wait until the queue is not full.
    if ( timeout_us == 0 )
        mutex_lock(&m_full_lock);
    else if ( ztimer_mutex_lock_timeout(ZTIMER_USEC, &m_full_lock, timeout_us) != 0 )
        return false;

    mutex_lock(&m_access_lock);
    m_buffer[m_push++ & (QUEUE_SIZE - 1)] = event;
    bool not_full = (m_push - m_pop) < QUEUE_SIZE;
    mutex_unlock(&m_access_lock);

    // Release m_full_lock unless the queue is full.
    if ( not_full )
        mutex_unlock(&m_full_lock);

    return true;
}

bool key_queue::try_pop(entry_t* pevent)
{
    mutex_lock(&m_access_lock);

    if ( m_pop == m_push ) {
        mutex_unlock(&m_access_lock);
        return false;
    }

    if ( pevent ) {
        *pevent = m_buffer[m_pop++ & (QUEUE_SIZE - 1)];
        // Here, reset the peek point!
        m_peek = m_pop;
    }

    mutex_unlock(&m_access_lock);
    mutex_unlock(&m_full_lock);  // No harm if m_full_lock was not locked.
    return true;
}

bool key_queue::try_peek(entry_t* pevent)
{
    mutex_lock(&m_access_lock);

    if ( m_peek == m_push ) {
        mutex_unlock(&m_access_lock);
        return false;
    }

    if ( pevent )
        *pevent = m_buffer[m_peek++ & (QUEUE_SIZE - 1)];

    mutex_unlock(&m_access_lock);
    return true;
}

// ( deferrer -- )
int key_queue::defer_start(lua_State* L)
{
    LOG_DEBUG("Map: start defer\n");
    assert( m_rdeferrer == LUA_NOREF );
    assert( main_thread::is_active() );

    m_rdeferrer = luaL_ref(L, LUA_REGISTRYINDEX);
    m_get = &key_queue::try_peek;
    return 0;
}

// ( -- )
int key_queue::defer_stop(lua_State* L)
{
    LOG_DEBUG("Map: stop defer\n");
    assert( m_rdeferrer != LUA_NOREF );
    assert( main_thread::is_active() );

    luaL_unref(L, LUA_REGISTRYINDEX, m_rdeferrer);
    m_rdeferrer = LUA_NOREF;
    m_get = &key_queue::try_pop;
    return 0;
}

// ( -- deferrer | nil )
int key_queue::defer_owner(lua_State* L)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_rdeferrer);
    return 1;
}

// ( slot_index1 is_press -- true | false )
int key_queue::defer_is_pending(lua_State* L)
{
    int slot_index1 = luaL_checkinteger(L, 1);
    bool is_press = lua_toboolean(L, 2);
    const entry_t event = {{ uint8_t(slot_index1), is_press }};

    mutex_lock(&m_access_lock);
    bool found = false;
    for ( size_t i = m_pop ; i != m_peek ; i++ )
        if ( m_buffer[i & (QUEUE_SIZE - 1)].value == event.value ) {
            found = true;
            break;
        }
    mutex_unlock(&m_access_lock);

    lua_pushboolean(L, found);
    return 1;
}

// ( -- )
int key_queue::defer_remove_last(lua_State*)
{
    mutex_lock(&m_access_lock);

    if ( m_pop == m_peek ) {
        mutex_unlock(&m_access_lock);
        return 0;
    }

    for ( size_t i = m_peek - 1 ; i != m_pop ; i-- )
        m_buffer[i & (QUEUE_SIZE - 1)] = m_buffer[(i - 1) & (QUEUE_SIZE - 1)];
    m_pop++;

    mutex_unlock(&m_access_lock);
    mutex_unlock(&m_full_lock);  // No harm if m_full_lock was not locked.
    return 0;
}
