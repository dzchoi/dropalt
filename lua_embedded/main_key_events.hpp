#pragma once

#include <cstdint>              // for uint8_t, uint16_t, ...

#include "mutex.h"              // for mutex_t



struct lua_State;

// Static class managing the key event queue between matrix_thread and main_thread.
// Note: No initialization is required.
class main_key_events {
public:
    union key_event_t {
        struct { uint8_t slot_index; bool is_press; };
        uint16_t uint16;
    };
    static_assert( sizeof(key_event_t) == sizeof(uint16_t) );

    // All methods in this class are thread safe using the mutex m_access_lock.

    // Push a key event onto the queue. If full it waits for an event to be popped off,
    // either indefinitely (timeout_us = 0) or within timeout_us.
    // Note: If the queue fills up exclusively with deferred events, push() will fail,
    // potentially causing the matrix_thread to hang. Increasing QUEUE_SIZE may resolve
    // this.
    static bool push(key_event_t event, uint32_t timeout_us =0);

    // Retrieve the next event from the queue and return true if successful. Do dry run
    // if pevent is NULL.
    static bool get(key_event_t* pevent =nullptr) { return m_get(pevent); }

    // Check if the queue is full with all deferred events.
    static bool terminal_full();

    // Start or stop defer mode.
    static int defer_start(lua_State* L);  // fw.defer_start(): void
    static int defer_stop(lua_State* L);   // fw.defer_stop(): void

    // Check if a key event is deferred on the given slot (i.e. if it was peeked).
    // fw.defer_is_pending(slot_index: int, is_press: bool): bool
    static int defer_is_pending(lua_State* L);

    // Remove the most recent deferred event from the queue.
    // fw.defer_remove_last(): void
    static int defer_remove_last(lua_State* L);

private:
    constexpr main_key_events() =delete;  // Ensure a static class

    static mutex_t m_access_lock;  // Locked when accessing the queue.
    static mutex_t m_full_lock;    // Locked when the queue is full.

    static key_event_t m_buffer[];

    // Starting indices for queue access operations
    // Events occurring between m_pop and m_peek are considered deferred.
    // Note: m_pop <= m_peek <= m_push <= QUEUE_SIZE
    static size_t m_push;
    static size_t m_peek;
    static size_t m_pop;

    // m_get() will execute try_pop() in normal mode, or try_peek() in defer mode.
    static bool (*m_get)(key_event_t*);

    // Pop off a key event from the queue if available. Do dry run if pevent is NULL.
    static bool try_pop(key_event_t* pevent);

    // Peek next event if available after the last peek. Do dry run if pevent is NULL.
    static bool try_peek(key_event_t* pevent);
};
