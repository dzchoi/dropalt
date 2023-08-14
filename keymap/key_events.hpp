#pragma once

#include <stdint.h>             // for uint8_t, uint32_t
#include "cond.h"               // for cond_t, cond_init()
#include "mutex.h"              // for mutex_t, mutex_init()



namespace key {

class defer_t;



class events_t {
public:
    events_t();

    // Not copyable nor movable.
    events_t(const events_t&) =delete;
    events_t& operator=(const events_t&) =delete;

    union key_event_t {
        struct { uint8_t index; bool is_press; };
        int16_t data;
    };
    static_assert( sizeof(key_event_t) == 2 );

    // All the methods are thread safe by using the mutex, m_lock.

    // Push a key event onto the buffer. If the buffer is full it waits for an existing
    // event being popped off, either indefinitely (timeout_us == 0) or within timeout_us.
    // Todo: If the buffer gets full with only deferred events push() will no longer be
    //  successful, possibly causing the client thread (matrix_thread) being stuck. One
    //  option then would be increasing BUFFER_SIZE.
    bool push(size_t index, bool is_press, uint32_t timeout_us =0);

    // Get the next event from buffer and return true if it is successful. Do dry run if
    // pevent is NULL.
    bool next_event(key_event_t* pevent =nullptr) {
        return (this->*m_next_event)(pevent);
    }

    // Start or stop Defer mode. See the comments in defer.hpp for the Defer mode.
    void start_defer(defer_t* pmap);
    void stop_defer();

    defer_t* deferrer() const { return m_deferrer; }

    // Check if the given event is deferred (i.e if it was peeked).
    bool is_deferred(size_t index, bool is_press) const;

    // Discard the last deferred event from the buffer.
    void discard_last_deferred();

private:
    mutable cond_t m_not_full;
    mutable mutex_t m_lock;

    static constexpr size_t BUFFER_SIZE = 16;
    key_event_t m_buffer[BUFFER_SIZE];

    // Starting index for each operation.
    // The events between m_pop and m_peek are called deferred.
    size_t m_push = 0;
    size_t m_peek = 0;
    size_t m_pop = 0;

    defer_t* m_deferrer = nullptr;

    // Pop off a key event from the buffer if available. Do dry run if pevent is NULL.
    bool try_pop(key_event_t* pevent);

    // Peek next event since previous peek if available. Do dry run if pevent is NULL.
    bool try_peek(key_event_t* pevent);

    // next_event() will execute try_pop() in normal mode, or try_peek() in Defer mode.
    bool (events_t::*m_next_event)(key_event_t*) = &events_t::try_pop;
};

}  // namespace key
