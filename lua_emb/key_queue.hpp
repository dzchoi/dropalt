#pragma once

#include <cstdint>              // for uint8_t, uint16_t, ...

#include "mutex.h"              // for mutex_t



struct lua_State;

// Static class managing the key event queue between matrix_thread and main_thread.
// Note: No initialization is required.
class key_queue {
public:
    union entry_t {
        struct { uint8_t slot_index1; bool is_press; };
        uint16_t value;
    };
    static_assert( sizeof(entry_t) == sizeof(uint16_t) );

    // All methods in this class are thread safe using the mutex m_access_lock.

    // Push a key event onto the queue. If full it waits for an event to be popped off,
    // either indefinitely (timeout_us = 0) or within timeout_us.
    // Note: If the queue fills up exclusively with deferred events, push() will fail,
    // potentially causing the matrix_thread to hang. Increasing QUEUE_SIZE may resolve
    // this.
    static bool push(unsigned slot_index1, bool is_press, uint32_t timeout_us =0);

    // Retrieve the next event from the queue and return true if successful. Do dry run
    // if pevent is NULL.
    static bool next_event(entry_t* pevent =nullptr) {  // implicitly inline
        return m_next_event(pevent);
    }

    // Start or stop defer mode.
    static int defer_start(lua_State* L);  // ( deferrer -- )
    static int defer_stop(lua_State* L);   // ( -- )

    static int defer_owner(lua_State* L);  // ( -- deferrer | nil )

    // Check if an event on the given slot is deferred (i.e. if it was peeked).
    static int defer_is_pending(lua_State* L);  // ( slot_index1 is_press -- true | false )

    // Remove the most recent deferred event from the queue.
    static int defer_remove_last(lua_State*);  // ( -- )

private:
    constexpr key_queue() =delete;  // Ensure a static class

    static mutex_t m_access_lock;  // Locked when accessing the queue.
    static mutex_t m_full_lock;    // Locked when the queue is full.

    static constexpr size_t QUEUE_SIZE = 16;
    static entry_t m_buffer[QUEUE_SIZE];

    // Starting indices for queue access operations
    // Events occurring between m_pop and m_peek are considered deferred.
    // Note: m_pop <= m_peek <= m_push <= QUEUE_SIZE
    static size_t m_push;
    static size_t m_peek;
    static size_t m_pop;

    // Reference to the Lua Defer instance who started the defer mode.
    static int m_rdeferrer;

    // next_event() will execute try_pop() in normal mode, or try_peek() in defer mode.
    static bool (*m_next_event)(entry_t*);

    // Pop off a key event from the queue if available. Do dry run if pevent is NULL.
    static bool try_pop(entry_t* pevent);

    // Peek next event if available after the last peek. Do dry run if pevent is NULL.
    static bool try_peek(entry_t* pevent);
};
