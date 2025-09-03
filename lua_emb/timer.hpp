#pragma once

#include "ztimer.h"

#include "event_ext.hpp"        // for event_ext_t



struct lua_State;

// Internal C++ helper for the `Timer` class in Lua. Wraps ztimer_t.
class _timer_t: public ztimer_t {
public:
    static int create(lua_State* L);  // ( -- userdata )

    // Note: This class is intentionally minimal and does not define an explicit
    // `destroy()` method. The callback reference (`m_rcallback`) is managed exclusively
    // through the `start()` and `stop()` methods invoked from Lua.

    static int start(lua_State* L);  // ( userdata timeout_ms callback [repeated] -- 0 )

    static int stop(lua_State* L);  // ( userdata -- bool )

    static int now(lua_State* L);  // ( userdata -- [int] )

protected:
    explicit _timer_t()
    : ztimer_t { .callback = _tmo_key_timer, .arg = this }
    {}

private:
    uint32_t m_timeout_ms = 0;

    uint32_t m_epoch = 0;

    uint32_t m_restart_ms = 0;

    int m_rcallback = LUA_NOREF;  // Reference to a callback function in Lua.

    event_ext_t<_timer_t*> m_event_timeout = { nullptr, _hdlr_timeout, this };
    static void _hdlr_timeout(event_t* event);

    static void _tmo_key_timer(void* arg);
};
