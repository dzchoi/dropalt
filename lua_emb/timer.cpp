#include <new>                  // for placement new
#include "assert.h"
#include "log.h"

extern "C" {
#include "lauxlib.h"
}

#include "lua.hpp"              // for lua::L
#include "main_thread.hpp"      // for main_thread::signal_event()
#include "timer.hpp"



int _timer_t::create(lua_State* L)
{
    uint32_t timeout_ms = luaL_checkinteger(L, 1);
    void* memory = lua_newuserdata(L, sizeof(_timer_t));
    new (memory) _timer_t(timeout_ms);
    // ( -- timeout_ms userdata )
    return 1;
}

int _timer_t::start(lua_State* L)
{
    // ( -- userdata callback )
    _timer_t* const that = static_cast<_timer_t*>(lua_touserdata(L, 1));
    assert( that != nullptr );

    luaL_checktype(L, 2, LUA_TFUNCTION);
    assert( that->m_rcallback == LUA_NOREF );
    that->m_rcallback = luaL_ref(L, LUA_REGISTRYINDEX);
    // ( -- userdata )

    ztimer_set(ZTIMER_MSEC, that, that->m_timeout_ms);
    return 0;
}

int _timer_t::stop(lua_State* L)
{
    _timer_t* const that = static_cast<_timer_t*>(lua_touserdata(L, 1));
    assert( that != nullptr );
    ztimer_remove(ZTIMER_MSEC, that);
    luaL_unref(L, LUA_REGISTRYINDEX, that->m_rcallback);
    that->m_rcallback = LUA_NOREF;  // Also indicates that timeout is not expected.
    // ( -- userdata )
    return 0;
}

int _timer_t::is_running(lua_State* L)
{
    _timer_t* const that = static_cast<_timer_t*>(lua_touserdata(L, 1));
    assert( that != nullptr );
    lua_pushboolean(L, ztimer_is_set(ZTIMER_MSEC, that) );
    // ( -- userdata bool )
    return 1;
}

// _hdlr_timeout() executs in the context of main_thread.
void _timer_t::_hdlr_timeout(event_t* event)
{
    _timer_t* const that = static_cast<event_ext_t<_timer_t*>*>(event)->arg;

    // Although ztimer_t guarantees that its callback is never invoked after
    // ztimer_remove() - even if the timer expires during ztimer_remove() - a spurious
    // timeout can still occur in rare cases. For example, our _timer_t might expire
    // after entering on_release(), but before calling fw.timer_stop(). In this window,
    // the m_event_timeout event could be signaled before _timer_t::stop() executes.
    // To guard against this, we explicitly check whether the timeout is expected
    // (that->m_rcallback != LUA_NOREF). If it isn't, we discard the event and prevent
    // on_timeout() from executing.
    if ( that->m_rcallback != LUA_NOREF ) {
        using lua::L;
        // Invoke the Lua callback function.
        lua_rawgeti(L, LUA_REGISTRYINDEX, that->m_rcallback);
        // ( -- callback )
        lua_call(L, 0, 0);
        luaL_unref(L, LUA_REGISTRYINDEX, that->m_rcallback);
        that->m_rcallback = LUA_NOREF;
        // ( -- )
    }

    else
        LOG_WARNING("Map: spurious timeout\n");
}

void _timer_t::_tmo_key_timer(void* arg)
{
    _timer_t* const that = static_cast<_timer_t*>(arg);
    // Use an event to execute the timeout handler (_hdlr_timeout()) in thread context,
    // rather than directly in interrupt context. This also ensures that on_timeout()
    // is invoked only when Lua is idle - not actively executing another function.
    main_thread::signal_event(&that->m_event_timeout);
}
