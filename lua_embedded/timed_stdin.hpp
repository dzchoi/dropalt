#pragma once

#include <cstddef>              // for size_t
#include <cstdint>              // for uint32_t, uint8_t



struct lua_State;
struct usbus_cdcacm_device;

extern "C" void cdc_acm_rx_pipe(usbus_cdcacm_device*, uint8_t* data, size_t len);

// This static class enables reading from stdin with a timeout.
// Note: It operates on stdin_isrpipe from stdio_base.h to manage stdin reading.
class timed_stdin {
public:
    static void enable();
    static void disable();
    static bool is_enabled() { return m_enabled; }

    // Indicate whether input is available in the input buffer.
    static bool has_input() { return m_read_ahead > 0; }

    // Wait for input from stdin until one of the following occurs:
    //   - input becomes available,
    //   - stop_read() is called, or
    //   - timeout_ms elapses.
    // Return true if input is available, or false in other cases. On timeout,
    // THREAD_FLAG_TIMEOUT is set on the current thread. If the thread already has a
    // pending signal in the mask, return immediately.
    // Note: m_read_ahead must be zero before calling this method, or existing input
    // may be overwritten.
    static bool wait_for_input(uint32_t timeout_ms, thread_flags_t mask);

    // Stop wait_for_input() from waiting even before the timeout expires.
    static void stop_wait();

    // Function used as a lua_Reader by the REPL to read input from stdin.
    static const char* _reader(lua_State* L, void* pdata, size_t* psize);

private:
    constexpr timed_stdin() =delete;  // Ensure a static class

    // Allow cdc_acm_rx_pipe() to access m_enabled.
    friend void cdc_acm_rx_pipe(usbus_cdcacm_device*, uint8_t* data, size_t len);

    // Read from stdin into the input buffer, returning when input becomes available,
    // stop_read() is called, or timeout_ms elapses. Return true if a timeout occurs,
    // false otherwise. If a signal in the given mask is already pending on the current
    // thread, returns false immediately without reading.
    static bool read_timed_out(uint32_t timeout_ms, thread_flags_t mask);

    // The stdin input buffer that will hold the bytecode for the REPL.
    static uint8_t m_read_buffer[];

    // Size of the stdin input data read ahead into m_read_buffer[].
    static size_t m_read_ahead;

    static bool m_enabled;

    static bool m_stoppable;
};
