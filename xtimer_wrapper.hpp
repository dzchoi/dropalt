#pragma once

#include "thread_flags.h"
#include "xtimer.h"

#include <utility>              // for std::forward<>



class xtimer_periodic_signal_t: xtimer_t {
public:
    // Set up (but not start) an xtimer that will trigger the given signal periodically.
    // Note that m_pthread is initialized to the thread that "owns" this timer, which can
    // be a different thread or even an ISR.
    xtimer_periodic_signal_t(uint32_t period_us,
        thread_t* pthread =thread_get_active(), thread_flags_t flags =THREAD_FLAG_TIMEOUT)
    : m_period_us(period_us), m_pthread(pthread), m_flags(flags)
    {
        callback = &_tmo_periodic_signal;
        arg = this;
    }

    // Start/restart it.
    void start(bool trigger_now =false) {
        xtimer_set(this, trigger_now ? 0 : m_period_us);
    }

    // Stop it.
    void stop() { xtimer_remove(this); }

    // Check if running now.
    bool is_running() const { return xtimer_is_set(this); }

    ~xtimer_periodic_signal_t() { stop(); }

    // Copies prohibited.
    xtimer_periodic_signal_t(const xtimer_periodic_signal_t&) =delete;
    void operator=(const xtimer_periodic_signal_t&) =delete;

private:
    const uint32_t m_period_us;
    thread_t* const m_pthread;
    const thread_flags_t m_flags;
    // uint32_t m_wakeup_us = xtimer_now_usec();

    static void _tmo_periodic_signal(void* arg) {
        xtimer_periodic_signal_t* const that =
            static_cast<xtimer_periodic_signal_t*>(arg);
        // Todo: Accurate waiting time based on m_wakeup_us.
        xtimer_set(that, that->m_period_us);
        thread_flags_set(that->m_pthread, that->m_flags);
    }
};



template <typename T>
class xtimer_periodic_callback_t: xtimer_t {
public:
    xtimer_periodic_callback_t(uint32_t period_us, void (*callback)(T), T&& arg)
    : m_period_us(period_us), m_callback(callback), m_arg(std::forward<T>(arg))
    {
        xtimer_t::callback = &_tmo_periodic_callback;
        xtimer_t::arg = this;
    }

    void start(bool trigger_now =false) {
        xtimer_set(this, trigger_now ? 0 : m_period_us);
    }

    void stop() { xtimer_remove(this); }

    bool is_running() const { return xtimer_is_set(this); }

    ~xtimer_periodic_callback_t() { stop(); }

    xtimer_periodic_callback_t(const xtimer_periodic_callback_t&) =delete;
    void operator=(const xtimer_periodic_callback_t&) =delete;

private:
    const uint32_t m_period_us;
    void (*const m_callback)(T);
    const T m_arg;

    static void _tmo_periodic_callback(void* arg) {
        xtimer_periodic_callback_t* const that =
            static_cast<xtimer_periodic_callback_t*>(arg);
        xtimer_set(that, that->m_period_us);
        (that->m_callback)(that->m_arg);
    }
};



class xtimer_onetime_signal_t: xtimer_t {
public:
    xtimer_onetime_signal_t(uint32_t timeout_us,
        thread_t* pthread =thread_get_active(), thread_flags_t flags =THREAD_FLAG_TIMEOUT)
    : m_timeout_us(timeout_us), m_pthread(pthread), m_flags(flags)
    {
        callback = &_tmo_onetime_signal;
        arg = this;
    }

    void start() { xtimer_set(this, m_timeout_us); }

    void stop() { xtimer_remove(this); }

    bool is_running() const { return xtimer_is_set(this); }

    ~xtimer_onetime_signal_t() { stop(); }

    xtimer_onetime_signal_t(const xtimer_onetime_signal_t&) =delete;
    void operator=(const xtimer_onetime_signal_t&) =delete;

private:
    const uint32_t m_timeout_us;
    thread_t* const m_pthread;
    const thread_flags_t m_flags;

    static void _tmo_onetime_signal(void* arg) {
        xtimer_onetime_signal_t* const that =
            static_cast<xtimer_onetime_signal_t*>(arg);
        thread_flags_set(that->m_pthread, that->m_flags);
    }
};



template <typename T>
class xtimer_onetime_callback_t: xtimer_t {
public:
    xtimer_onetime_callback_t(uint32_t timeout_us, void (*callback)(T), T&& arg)
    : m_timeout_us(timeout_us), m_callback(callback), m_arg(std::forward<T>(arg))
    {
        xtimer_t::callback = &_tmo_onetime_callback;
        xtimer_t::arg = this;
    }

    void start() { xtimer_set(this, m_timeout_us); }

    void stop() { xtimer_remove(this); }

    bool is_running() const { return xtimer_is_set(this); }

    ~xtimer_onetime_callback_t() { stop(); }

    xtimer_onetime_callback_t(const xtimer_onetime_callback_t&) =delete;
    void operator=(const xtimer_onetime_callback_t&) =delete;

private:
    const uint32_t m_timeout_us;
    void (*const m_callback)(T);
    const T m_arg;

    static void _tmo_onetime_callback(void* arg) {
        xtimer_onetime_callback_t* const that =
            static_cast<xtimer_onetime_callback_t*>(arg);
        (that->m_callback)(that->m_arg);
    }
};
