#pragma once

#include <cstdint>              // for uint8_t, uint16_t, ...

#include "mutex.h"              // for mutex_t



namespace key {

class defer_t;



// Static class managing the key event queue between matrix_thread and main_thread.
// Note: No initialization is required.
class event_queue {
public:
    union entry_t {
        struct { uint8_t slot_index; bool is_press; };
        uint16_t value;
    };
    static_assert( sizeof(entry_t) == sizeof(uint16_t) );

    // All methods in this class are thread safe using the mutex m_access_lock.

    // Push a key event onto the queue. If full it waits for an event to be popped off,
    // either indefinitely (timeout_us = 0) or within timeout_us.
    // Note: If the queue fills up exclusively with deferred events, push() will fail,
    // potentially causing the matrix_thread to hang. Increasing QUEUE_SIZE may resolve
    // this.
    static bool push(unsigned slot_index, bool is_press, uint32_t timeout_us =0);

    // Retrieve the next event from the queue and return true if successful. Do dry run
    // if pevent is NULL.
    static bool next_event(entry_t* pevent =nullptr) {  // implicitly inline
        return m_next_event(pevent);
    }

    // Start of stop defer mode.
    // [Defer mode] ???
    // While in defer mode, all key events - except those acting on the "deferrer"
    // (entry_t::m_deferrer) - are deferred instead of being processed immediately. These
    // events do not trigger their own on_press/release() methods as they would in normal
    // mode; instead, they invoke on_other_press/release() of the deferrer. Therefore,
    // during defer mode, all key events are directed exclusively to the deferrer.
    // Nested deferring is not allowed and there is only one deferrer.

    // If on_other_press/release() returns false, the deferred events remain queued and
    // will only be processed once the deferrer stops defer mode, invoking their
    // respective on_press/release() methods. If on_other_press/release() returns true,
    // the deferred events will be processed immediately rather than remaining deferred.
    // The deferrer's own key events that occur during defer mode are also processed
    // immediately, invoking the deferrer's on_press/release() instead of on_other_press/
    // release().

    // Note: Those two cases of executing events immediately during defer mode can
    // disrupt the order of key event occurrences. See the comments in tap_hold.hpp.
    static void start_defer(defer_t* pmap);
    static void stop_defer();

    static defer_t* deferrer() { return m_deferrer; }

    // Check if the given event is deferred (i.e. if it was peeked).
    static bool is_deferred(unsigned slot_index, bool is_press);

    // Remove the most recent deferred event from the queue.
    static void remove_last_deferred();

private:
    constexpr event_queue() =delete;  // Ensure a static class

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

    // Who has initiated the defer mode.
    static defer_t* m_deferrer;

    // next_event() will execute try_pop() in normal mode, or try_peek() in defer mode.
    static bool (*m_next_event)(entry_t*);

    // Pop off a key event from the queue if available. Do dry run if pevent is NULL.
    static bool try_pop(entry_t* pevent);

    // Peek next event if available after the last peek. Do dry run if pevent is NULL.
    static bool try_peek(entry_t* pevent);
};

}
