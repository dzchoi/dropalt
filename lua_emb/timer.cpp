#include <new>                  // for placement new
#include "assert.h"
#include "log.h"

#include "lua.hpp"              // for lua::global_lua_state
#include "main_thread.hpp"      // for main_thread::signal_event()
#include "timer.hpp"



int _timer_t::create(lua_State* L)
{
    void* memory = lua_newuserdata(L, sizeof(_timer_t));
    new (memory) _timer_t();
    // ( -- userdata )
    return 1;
}

int _timer_t::start(lua_State* L)
{
    // ( -- userdata timeout_ms callback [repeated] )
    _timer_t* const that = static_cast<_timer_t*>(lua_touserdata(L, 1));
    assert( that != nullptr );

    const int timeout_ms = luaL_checkinteger(L, 2);
    assert( timeout_ms > 0 );

    // Note that lua_toboolean(L, 4) returns false if the argument is absent or nil.
    that->m_timeout_ms = lua_toboolean(L, 4) ? timeout_ms : 0;
    lua_settop(L, 3);
    // ( -- userdata timeout_ms callback )

    luaL_checktype(L, 3, LUA_TFUNCTION);
    if ( that->m_rcallback != LUA_NOREF ) {
        // This case can happen when restarting the timer.
        luaL_unref(L, LUA_REGISTRYINDEX, that->m_rcallback);
    }
    that->m_rcallback = luaL_ref(L, LUA_REGISTRYINDEX);
    // ( -- userdata timeout_ms )

    // Set the epoch to the current time.
    that->m_epoch = ztimer_set(ZTIMER_MSEC, that, timeout_ms);

    // Return 0 as the initial elapsed time since the epoch.
    lua_pushinteger(L, 0);
    return 1;
}

int _timer_t::stop(lua_State* L)
{
    _timer_t* const that = static_cast<_timer_t*>(lua_touserdata(L, 1));
    assert( that != nullptr );
    const bool result = ztimer_remove(ZTIMER_MSEC, that);
    luaL_unref(L, LUA_REGISTRYINDEX, that->m_rcallback);
    that->m_rcallback = LUA_NOREF;  // Also indicates that timeout is not expected.
    lua_pushboolean(L, result);
    // ( -- userdata result )
    return 1;
}

int _timer_t::now(lua_State* L)
{
    _timer_t* const that = static_cast<_timer_t*>(lua_touserdata(L, 1));
    assert( that != nullptr );
    if ( ztimer_is_set(ZTIMER_MSEC, that) ) {
        lua_pushinteger(L, ztimer_now(ZTIMER_MSEC) - that->m_epoch);
        // ( -- userdata int )
        return 1;
    }
    return 0;
}

// _hdlr_timeout() executes in the context of main_thread.
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
        lua::global_lua_state L;

        // Invoke the Lua callback function.
        lua_rawgeti(L, LUA_REGISTRYINDEX, that->m_rcallback);
        // ( -- callback )

        // Invoke the callback with an appropriate argument.
        if ( that->m_timeout_ms > 0 ) {
            // Safe to use ztimer_now() here: timers are scheduled back-to-back, with
            // each new timer set from within the handler itself.
            lua_pushinteger(L, ztimer_now(ZTIMER_MSEC) - that->m_epoch);
            lua_call(L, 1, 1);
            if ( lua_toboolean(L, -1) )
                that->m_epoch = that->m_restart_ms;
            lua_settop(L, 0);
        }
        else {
            lua_call(L, 0, 0);
            luaL_unref(L, LUA_REGISTRYINDEX, that->m_rcallback);
            that->m_rcallback = LUA_NOREF;
        }
        // ( -- )
    }

    else
        LOG_WARNING("Map: spurious timeout\n");
}

void _timer_t::_tmo_key_timer(void* arg)
{
    _timer_t* const that = static_cast<_timer_t*>(arg);

    // Start the timer again if it is a repeated one.
    if ( that->m_timeout_ms > 0 )
        that->m_restart_ms = ztimer_set(ZTIMER_MSEC, that, that->m_timeout_ms);

    // Use an event to execute the timeout handler (_hdlr_timeout()) in thread context,
    // rather than directly in interrupt context. This also ensures that on_timeout()
    // is invoked only when Lua is idle - not actively executing another function.
    main_thread::signal_event(&that->m_event_timeout);
}
