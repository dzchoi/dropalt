#pragma once

#include "mutex.h"              // for mutex_lock(), mutex_unlock()
#include "seeprom.h"            // for seeprom_flush(), seeprom_addr()
#include "ztimer.h"             // for ztimer_set()

#include <cstdint>              // for uint16_t
#include <cstddef>              // for size_t
#include <utility>              // for std::pair<>
#include <type_traits>          // for std::is_pod<>
#include "config.hpp"           // for NVM_WRITE_DELAY_MS



// SEEPROM is structured as a single associative array, providing efficient storage and
// retrieval of name-value pairs. Internally, the SEEPROM space (4K) is organized as a
// sequential structure where name-value pairs are stored contiguously, with all fields
// laid out back-to-back without any alignment or padding to optimize space usage.
// Writes to SEEPROM are performed in buffered mode to optimize wear leveling. Buffered
// data is automatically committed to SEEPROM either after a predefined delay
// (NVM_WRITE_DELAY_MS) or immediately upon being read back.
// Note: it employs a simple memory allocation mechanism (Tail Reclamation). To
// defragment, clear all entries using remove_all().

// Layout:
// SEEPROM_ADDR (0x44000000)
//  +0: nvm_occupied (2 bytes) - The amount of SEEPROM space currently in use.
//  +2: len of the first name-value pair (1 byte)
//  +3: null-terminated name string of size n
//  +3+n: value of size m - The data associated with the name.
//  +3+n+m: ... - The next name-value pair starts here, following the same structure.

// Considerations:
// - Each name-value pair must not exceed a total size of 255 bytes.
// - Values may have variable sizes but must adhere to the Plain Old Data (POD) type.
// - Do not exceed the maximum SEEPROM capacity of 4KB.
// - Ensure consistent use of the same value type T for both get() and set() operations.

class persistent {
public:
    static void init();

    // Retrieve the value associated with the given name. If the name-value pair does
    // not exist the function returns false, leaving the reference parameter unchanged.
    template <typename T>
    static bool get(const char* name, T& value) {
        static_assert( std::is_pod_v<T> );
        return get(name, &value, sizeof(T));
    }

    static bool get(const char* name, void* value, size_t value_size);

    // Assign a value to the given name. If the name-value pair does not exist a new
    // entry is created with the given value.
    // Ensure that the type T used between get() and set() remains consistent.
    template <typename T>
    static bool set(const char* name, const T& value) {
        static_assert( std::is_pod_v<T> );
        return set(name, &value, sizeof(T));
    }

    static bool set(const char* name, const void* value, size_t value_size);

    // Remove the name-value pair associated with the given name.
    static bool remove(const char* name);

    // Remove all entries.
    static void remove_all();

    // Walk through names.
    static const char* next(const char* name);

private:
    constexpr persistent() =delete;  // Ensure a static class

    // Create a new name-value pair in the SEEPROM and return a pointer to the value
    // field.
    static void* create(const char* name, size_t value_size);

    // If the given name exists, return a pair consisting of a pointer to the value field
    // and its size; otherwise, returns (NULL, 0).
    static std::pair<void*, size_t> find(const char* name);

    // Commit the change now.
    static void commit_now() { seeprom_flush(); }

    // Commit the change in SEEPROM in NVM_WRITE_DELAY_MS.
    static void commit_later() {
        static ztimer_t timer = {
            .callback = [](void*) { commit_now(); },
            .arg = nullptr
        };
        ztimer_set(ZTIMER_MSEC, &timer, NVM_WRITE_DELAY_MS);
    }

    class lock_guard {
    public:
        explicit lock_guard() { mutex_lock(&m_mutex); }
        ~lock_guard() { mutex_unlock(&m_mutex); }

    private:
        static mutex_t m_mutex;
    };

    // Total (virtual) size of the SEEPROM.
    static const size_t m_size;

    // Size of the free space in SEEPROM, located at the start of SEEPROM.
    // Note that &nvm_occupied can be used to get the starting address of SEEPROM. It
    // doesn't need to be a volatile pointer, as the SEEPROM is accessed through a mutex,
    // and if data is not committed yet any read attempt will trigger an immediate commit.
    static uint16_t& nvm_occupied;

    static uint8_t* begin() {
        return static_cast<uint8_t*>(seeprom_addr(sizeof(nvm_occupied)));
    }

    static uint8_t* end() {
        return static_cast<uint8_t*>(seeprom_addr(nvm_occupied));
    }
};
