#include "compiler_hints.h"     // for likely()
#include "isrpipe.h"            // for isrpipe_write(), tsrb_get(), ...
#include "mutex.h"              // for mutex_lock(), mutex_unlock()
#include "stdio_base.h"         // for stdin_isrpipe
#include "thread.h"             // for thread_get_active()
#include "thread_flags.h"       // for thread_flags_set(), thread_flags_clear()
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



uint8_t timed_stdin::m_read_buffer[CONFIG_STDIN_RX_BUFSIZE]
    __attribute__((section(".noinit"), aligned(sizeof(uint32_t))));

size_t timed_stdin::m_read_ahead = 0;

// The stdin will be enabled only when Lua REPL DTE is ready.
bool timed_stdin::m_enabled = false;

bool timed_stdin::m_waiting_for_input = false;

// Override the weak cdc_acm_rx_pipe() in riot/sys/usb/usbus/cdc/acm/cdc_acm_stdio.c,
// which is called from usb_thread context.
void cdc_acm_rx_pipe(usbus_cdcacm_device*, uint8_t* data, size_t len)
{
    if ( likely(data) ) {
        // Note that the Linux CDC ACM driver has an issue where it echoes characters
        // received between the opening of the TTY and the disabling of ECHO:
        //   - https://lore.kernel.org/linux-serial/d6d376ceb45b5a72c2a053721eabeddfa11cc1a5.camel@infinera.com/
        //   - https://michael.stapelberg.ch/posts/2021-04-27-linux-usb-virtual-serial-cdc-acm/
        // This can cause early stdout messages to be looped back into stdin while the
        // DTE is still establishing a connection. We avoid this problem by disabling
        // stdin until a ping packet is received from the DTE.

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



void timed_stdin::enable()
{
    assert( m_read_ahead == 0 );
    m_enabled = true;
}

void timed_stdin::disable()
{
    unsigned state = irq_disable();
    m_enabled = false;
    tsrb_clear(&stdin_isrpipe.tsrb);
    m_read_ahead = 0;
    stop_wait();
    irq_restore(state);
}

void timed_stdin::stop_wait()
{
    if ( m_waiting_for_input )
        mutex_unlock(&stdin_isrpipe.mutex);
}

bool timed_stdin::wait_for_input(uint32_t timeout_ms)
{
    if ( m_read_ahead > 0 )
        return true;

    m_waiting_for_input = true;
    mutex_lock(&stdin_isrpipe.mutex);  // Does not block.

    // Note that the call to tsrb_get() and the check of thread_get_active()->flags must
    // occur after the first mutex_lock(). This ensures that any subsequent tsrb pushes
    // (via isrpipe_write()) or thread flag updates (via stop_wait()) can release the
    // mutex, allowing the second mutex_lock() to return immediately.
    m_read_ahead = tsrb_get(
        &stdin_isrpipe.tsrb, m_read_buffer, sizeof(m_read_buffer));
    thread_flags_clear(THREAD_FLAG_TIMEOUT);

    if ( m_read_ahead == 0 && thread_get_active()->flags == 0 ) {
        ztimer_t timer = {
            .callback = [](void* arg) {
                thread_flags_set((thread_t*)arg, THREAD_FLAG_TIMEOUT);
                mutex_unlock(&stdin_isrpipe.mutex);
            },
            .arg = thread_get_active()
        };
        ztimer_set(ZTIMER_MSEC, &timer, timeout_ms);
        mutex_lock(&stdin_isrpipe.mutex);  // Zzz

        ztimer_remove(ZTIMER_MSEC, &timer);
        m_read_ahead = tsrb_get(
            &stdin_isrpipe.tsrb, m_read_buffer, sizeof(m_read_buffer));
    }

    mutex_unlock(&stdin_isrpipe.mutex);
    m_waiting_for_input = false;
    return m_read_ahead > 0;
}

const char* timed_stdin::_reader(lua_State*, void* timeout_ms, size_t* psize)
{
    if ( m_read_ahead == 0 ) {
        mutex_lock(&stdin_isrpipe.mutex);  // Does not block.

        m_read_ahead = tsrb_get(
            &stdin_isrpipe.tsrb, m_read_buffer, sizeof(m_read_buffer));

        if ( m_read_ahead == 0 ) {
            ztimer_t timer = {
                .callback = [](void*) { mutex_unlock(&stdin_isrpipe.mutex); },
                .arg = nullptr
            };
            ztimer_set(ZTIMER_MSEC, &timer, (uint32_t)(uintptr_t)timeout_ms);
            mutex_lock(&stdin_isrpipe.mutex);  // Zzz

            ztimer_remove(ZTIMER_MSEC, &timer);
            m_read_ahead = tsrb_get(
                &stdin_isrpipe.tsrb, m_read_buffer, sizeof(m_read_buffer));
        }

        mutex_unlock(&stdin_isrpipe.mutex);

        if ( m_read_ahead == 0 )
            LOG_ERROR("Lua: stdin is not available, failing REPL.\n");
    }

    *psize = m_read_ahead;
    m_read_ahead = 0;
    return (const char*)m_read_buffer;
}
