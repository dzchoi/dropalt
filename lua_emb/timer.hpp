#pragma once

#include "ztimer.h"

#include "event_ext.hpp"        // for event_ext_t



struct lua_State;

// Internal C++ helper for the `Timer` class in Lua. Wraps ztimer_t.
class _timer_t: public ztimer_t {
public:
    static int create(lua_State* L);  // ( timeout_ms -- userdata )

    // Note: No explicit counterpart to create() is provided, as this class is
    // intentionally simple. The callback reference (m_rcallback) is managed exclusively
    // by start() and stop() from Lua.

    static int start(lua_State* L);  // ( userdata callback -- )

    static int stop(lua_State* L);  // ( userdata -- )

    static int is_running(lua_State* L);  // ( userdata -- bool )

protected:
    explicit _timer_t(uint32_t timeout_ms)
    : ztimer_t { .callback = _tmo_key_timer, .arg = this }
    , m_timeout_ms(timeout_ms) {}

private:
    const uint32_t m_timeout_ms;

    event_ext_t<_timer_t*> m_event_timeout = { nullptr, _hdlr_timeout, this };
    static void _hdlr_timeout(event_t* event);

    static void _tmo_key_timer(void* arg);

    int m_rcallback = LUA_NOREF;  // Reference to a callback function in Lua.
};
