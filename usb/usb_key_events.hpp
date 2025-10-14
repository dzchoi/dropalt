#pragma once

#include <cstdint>              // for uint8_t, uint16_t

#include "mutex.h"              // for mutex_t



class usb_key_events {
public:
    usb_key_events(mutex_t& mutex): m_not_full(mutex) {}

    struct key_event_t { uint8_t keycode; bool is_press; };
    static_assert( sizeof(key_event_t) == sizeof(uint16_t) );

    // These methods are NOT thread-safe. It is the caller's responsibility to ensure
    // safe access and synchronization.
    // - push() is invoked by report_event() from the client thread (main_thread),
    //   with interrupts disabled.
    // - The remaining methods are used exclusively by usb_thread, which runs at the
    //   highest priority and is not preempted.

    void push(key_event_t event, bool wait_if_full =false);
    bool pop();
    void clear();

    bool not_empty() const { return m_begin != m_end; }
    bool not_full() const { return (m_end - m_begin) < QUEUE_SIZE; }
    bool peek(key_event_t& event) const;

private:
    // The original usbus_hid_device_t::in_lock is repurposed as m_not_full, used to
    // prevent queue overflow by locking when full. It no longer guards concurrent
    // access to usbus_hid_device_t::in_buf, as submit_report() is exclusively called
    // from usb_thread (not from any lower-priority thread or from interrupts), ensuring
    // inherent thread safety without additional locking.
    mutex_t& m_not_full;  // must be initialized already.

    static constexpr size_t QUEUE_SIZE = 32;  // must be a power of two.
    static_assert( (QUEUE_SIZE > 0) && ((QUEUE_SIZE & (QUEUE_SIZE - 1)) == 0) );

    key_event_t m_events[QUEUE_SIZE];

    size_t m_begin = 0;
    size_t m_end = 0;
};
