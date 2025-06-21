#include "compiler_hints.h"     // for likely()
#include "isrpipe.h"            // for isrpipe_write(), tsrb_get(), ...
#include "mutex.h"              // for mutex_lock(), mutex_unlock()
#include "stdio_base.h"         // for stdin_isrpipe, STDIO_RX_BUFSIZE
#include "thread.h"             // for thread_get_active()
#include "thread_flags.h"       // for thread_flags_set()
#include "usbus_ext.h"          // Avoid compile error in acm.h below.
#include "usb/usbus/cdc/acm.h"  // for USBUS_CDC_ACM_LINE_STATE_DTE
#include "ztimer.h"             // for ztimer_set(), ztimer_remove()

#include "config.hpp"           // for ENABLE_CDC_ACM, ENABLE_LUA_REPL
#include "lua.hpp"              // for l_message(), lua::ping
#include "main_thread.hpp"      // for signal_dte_state(), signal_dte_ready()
#include "timed_stdin.hpp"

// Note: This file is compiled only if ENABLE_CDC_ACM is true.
// `cdc_acm_rx_pipe()` is included in the build when ENABLE_CDC_ACM is true, while all
// other functions are included only when ENABLE_LUA_REPL is true, using link-time
// optimization.
static_assert( ENABLE_CDC_ACM );



char timed_stdin::m_read_buffer[STDIO_RX_BUFSIZE]
    __attribute__((section(".noinit"), aligned(sizeof(uint32_t))));

size_t timed_stdin::m_read_ahead = 0;

// The stdin will be enabled only when Lua REPL DTE is ready.
bool timed_stdin::m_enabled = false;

// Override the weak cdc_acm_rx_pipe() in riot/sys/usb/usbus/cdc/acm/cdc_acm_stdio.c.
void cdc_acm_rx_pipe(usbus_cdcacm_device*, uint8_t* data, size_t len)
{
    if ( likely(data) ) {
        // Note that the Linux CDC ACM driver has an issue where it echoes any characters
        // received between the opening of the TTY and the disabling of ECHO:
        // https://lore.kernel.org/linux-serial/d6d376ceb45b5a72c2a053721eabeddfa11cc1a5.camel@infinera.com/
        // This behavior can result in early stdout messages being read back to stdin
        // while the DTE is establishing a connection. We resolve this problem by
        // disabling the stdin until a ping packet is received from DTE.

        if ( timed_stdin::m_enabled )
            isrpipe_write(&stdin_isrpipe, data, len);

        else if constexpr ( ENABLE_LUA_REPL ) {
            if ( len == sizeof(lua::ping) ) {
                lua::status_t status = ((lua::ping*)(void*)data)->status();
                if ( status != LUA_NOSTATUS )
                    main_thread::signal_dte_ready((uint8_t)status);
            }
        }
    }
    else
        main_thread::signal_dte_state(len == USBUS_CDC_ACM_LINE_STATE_DTE);
}

// Attempt to read up to `len` bytes from stdin into `buffer` within `timeout_ms`, and
// return the number of bytes read. If a timeout occurs or if stop_read() is called, the
// read is aborted and 0 is returned, signaling the calling thread with
// THREAD_FLAG_TIMEOUT.
// Note: If a signal is pending on the calling thread, the function returns immediately.
static size_t read_timeout(void* buffer, size_t len, uint32_t timeout_ms)
{
    mutex_lock(&stdin_isrpipe.mutex);

    int res = tsrb_get(&stdin_isrpipe.tsrb, (uint8_t*)buffer, len);
    if ( res == 0 && thread_get_active()->flags == 0 ) {
        ztimer_t timer = {
            .callback = [](void* arg) {
                thread_flags_set((thread_t*)arg, THREAD_FLAG_TIMEOUT);
                mutex_unlock(&stdin_isrpipe.mutex);
            },
            .arg = thread_get_active()
        };
        ztimer_set(ZTIMER_MSEC, &timer, timeout_ms);

        // This second mutex_lock() does wait until the mutex is unlocked elsewhere,
        // unlike the first mutex_lock(), which does not wait. If tsrb is pushed and the
        // mutex is unlocked between the first and this second mutex_lock(), the second
        // mutex_lock() will succeed immediately, allowing tsrb_get() to be checked again.
        mutex_lock(&stdin_isrpipe.mutex);  // Zzz
        ztimer_remove(ZTIMER_MSEC, &timer);
        res = tsrb_get(&stdin_isrpipe.tsrb, (uint8_t*)buffer, len);
    }

    mutex_unlock(&stdin_isrpipe.mutex);
    return res;
}



void timed_stdin::stop_read()
{
    mutex_unlock(&stdin_isrpipe.mutex);
}

void timed_stdin::enable()
{
    tsrb_clear(&stdin_isrpipe.tsrb);
    m_read_ahead = 0;
    m_enabled = true;
}

void timed_stdin::disable()
{
    m_enabled = false;
    stop_read();
}

bool timed_stdin::wait_for_input(uint32_t timeout_ms)
{
    if ( m_read_ahead > 0 )
        return true;

    m_read_ahead = read_timeout(m_read_buffer, sizeof(m_read_buffer), timeout_ms);
    return m_read_ahead > 0;
}

const char* timed_stdin::_reader(lua_State*, void* timeout_ms, size_t* psize)
{
    if ( !wait_for_input((uint32_t)(uintptr_t)timeout_ms) )
        l_message("Lua: stdin is not available, failing REPL.");

    *psize = m_read_ahead;
    m_read_ahead = 0;
    return m_read_buffer;
}
