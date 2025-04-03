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

    // Wait for input from stdin for `timeout_ms`. Return false if a timeout occurs or
    // stop_read() is invoked, and set the THREAD_FLAG_TIMEOUT flag on the current thread
    // in case of a timeout.
    static bool wait_for_input(uint32_t timeout_ms);

    // Stop read_timeout() from waiting even before the timeout expires.
    static void stop_read();

    // Function that can be used for a lua_Reader.
    static const char* _reader(lua_State* L, void* pdata, size_t* psize);

private:
    constexpr timed_stdin() =delete;  // Ensure a static class

    // Allow cdc_acm_rx_pipe() to access m_enabled.
    friend void cdc_acm_rx_pipe(usbus_cdcacm_device*, uint8_t* data, size_t len);

    // The stdin input buffer that will hold the bytecode for the REPL.
    static char m_read_buffer[];

    // Size of the stdin input data read ahead into m_read_buffer[].
    static size_t m_read_ahead;

    static bool m_enabled;
};
