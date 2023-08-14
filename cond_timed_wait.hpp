#pragma once

#include "cond.h"               // for cond_t, cond_wait()
#include "thread.h"             // for thread_getpid()
#include "ztimer.h"             // for ztimer_t, ztimer_now(), ztimer_set_wakeup(), ...



/**
 * @brief blocks the current thread until the condition variable is awakened or after
 *        the specified timeout duration
 *
 * @param[in] clock         ztimer clock for the timout.
 * @param[in] cond          Condition variable to wait on.
 * @param[in] mutex         Mutex object held by the current thread.
 * @param[in] timeout       Timeout duration according to the `clock`.
 *
 * @retval  true            if the condition variable is signaled before timeout.
 * @retval  false           if it timed out.
 */
// It's not the best to define this as inline in header file, but there is only one
// invoke (in key_events.cpp).
static inline bool cond_timed_wait(
    ztimer_clock_t* clock, cond_t* cond, mutex_t* mutex, uint32_t timeout)
{
    ztimer_t timer = {};
    const uint32_t since = ztimer_now(clock);
    ztimer_set_wakeup(clock, &timer, timeout, thread_getpid());
    cond_wait(cond, mutex);  // Zzz

    // No way to know here if we woke up due to the condition being signaled. We can only
    // check the elapsed time.
    const bool result = int32_t(ztimer_now(clock) - since) < int32_t(timeout);
    ztimer_remove(clock, &timer);
    return result;
}
