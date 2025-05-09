#pragma once

#include "mutex.h"              // for mutex_lock(), mutex_unlock()
#include "seeprom.h"            // for seeprom_flush()
#include "ztimer.h"             // for ztimer_set()

#include <cstdint>              // for uint16_t
#include <cstddef>              // for size_t
#include <type_traits>          // for std::is_pod<>
#include "config.hpp"           // for NVM_WRITE_DELAY_MS



// SEEPROM is structured as a single associative array, providing efficient storage and
// retrieval of name-value pairs. Internally, the 4K SEEPROM space is managed using the
// "Buddy" system, which prevents performance degradation from gradual defragmentation.
// See Dynamic Storage Allocation in *The Art of Computer Programming* for reference.
// Writes to SEEPROM are performed in buffered mode to optimize wear leveling. Buffered
// data is automatically committed to SEEPROM either after a predefined delay
// (NVM_WRITE_DELAY_MS) or immediately upon being read back.

// Structure of each name-value pair:
//  +0: block_header (1 byte)
//  +1: null-terminated name string (n bytes)
//  +1+n: value corresponding to the name. (POD of size 1-6 bytes, float, or const char*)

struct lua_State;

namespace lua {
    int _nvm_index(lua_State*);
    int _nvm_newindex(lua_State*);
}

class persistent {
public:
    static void init();

    // Retrieve the value associated with the given name. If the name-value pair does
    // not exist the function returns false, leaving the reference parameter unchanged.
    template <typename T>
    static bool get(const char* name, T& value);

    static const char* get(const char* name);

    // Assign a value to the given name. If the name-value pair does not exist a new
    // entry is created with the given value.
    // Ensure that the type T used between get() and set() remains consistent.
    template <typename T>
    static bool set(const char* name, const T& value);

    static bool set(const char* name, float value);

    static bool set(const char* name, const char* value);

    // Remove all entries.
    static void remove_all();

    // Walk through names.
    static const char* next(const char* name);

    // Number of entries.
    static size_t size();

private:
    constexpr persistent() =delete;  // Ensure a static class

    friend int lua::_nvm_index(lua_State*);
    friend int lua::_nvm_newindex(lua_State*);

    // Assert SEEPROM_SIZE is a power of 2.
    static_assert( SEEPROM_SIZE > 0 && ((SEEPROM_SIZE & (SEEPROM_SIZE - 1)) == 0) );
    static constexpr uint8_t SEEPROM_SIZE2 = __builtin_ctz(SEEPROM_SIZE);

    static uint8_t (&nvm)[SEEPROM_SIZE];  // will map to the SEEPROM.
    static_assert( sizeof(nvm) == SEEPROM_SIZE );

    enum {
        TSTRING = 0,    // const char*
        T1      = 1,    // bool, char, uint8_t, ...
        T2      = 2,    // short, ...
        T3      = 3,
        T4      = 4,    // int, ...
        T5      = 5,
        T6      = 6,
        TFLOAT  = 7,    // float, ...
        TFREE   = 7
    };

    union block_header {
        constexpr block_header(uint8_t size2, uint8_t value_type =TFREE, bool free =true)
        : size2(size2), value_type(value_type), free(free) {}

        uint8_t uint8;
        struct {
            uint8_t size2: 4;  // log2() of the byte size (2 <= size2 <= 12)
            uint8_t value_type: 3;
            uint8_t free: 1;  // will become MSB.
        };
    };
    static_assert( sizeof(block_header) == sizeof(uint8_t) );

    class iterator {
    private:
        block_header* p;

    public:
        constexpr iterator(void* p): p((block_header*)p) {}

        iterator(const iterator&) =default;
        iterator(iterator&&) =default;
        iterator& operator=(const iterator&) =default;
        iterator& operator=(iterator&&) =default;

        bool operator==(const iterator& other) const { return p == other.p; }
        bool operator!=(const iterator& other) const { return p != other.p; }

        block_header& operator*() { return *p; }
        block_header* operator->() { return p; }
        operator char*() { return (char*)p; }

        iterator& operator++() { p += 1 << p->size2; return *this; }
        iterator operator++(int) { block_header* q = p; p += 1 << p->size2; return q; }

        // `iterator operator+(int offset) const` is not provided; instead, the iterator
        // will be converted to `char*` for pointer arithmetic.

        void* pvalue() const { return p + 1 + __builtin_strlen((char*)p + 1) + 1; }

        // SEEPROM_ADDR must end with enough 0s to enable bit operations with the block
        // sizes in the range [2^0, 2^SEEPROM_SIZE2).
        static_assert( __builtin_ctz(SEEPROM_ADDR) >= SEEPROM_SIZE2 );

        iterator buddy(size_t blk_size) const {
            return (block_header*)((uintptr_t)p ^ blk_size);
        }
        iterator parent(size_t blk_size) const {
            return (block_header*)((uintptr_t)p & ~blk_size);
        }
    };

    static iterator begin() { return &nvm[0]; }
    static iterator end() { return &nvm[SEEPROM_SIZE]; }

    // Create a new name-value pair in the SEEPROM.
    static bool _create(const char* name, const void* value, size_t value_size, uint8_t value_type);

    // Remove the name-value pair corresponding to the given iterator.
    static bool _remove(iterator it);

    // Locate the name-value pair matching the given name and return the corresponding
    // iterator. If not found, return end().
    static iterator _find(const char* name);

    static bool _get(const char* name, void* value, size_t value_size);

    static bool _set(const char* name, const void* value, size_t value_size);

    // Commit the change now.
    static void _commit_now() { seeprom_flush(); }

    // Commit the change in SEEPROM in NVM_WRITE_DELAY_MS.
    static void _commit_later() {
        static ztimer_t timer = {
            .callback = [](void*) { _commit_now(); },
            .arg = nullptr
        };
        ztimer_set(ZTIMER_MSEC, &timer, NVM_WRITE_DELAY_MS);
    }

    class lock_guard {
    public:
        explicit lock_guard() { lock(); }
        ~lock_guard() { unlock(); }

        static void lock() { mutex_lock(&m_mutex); }
        static void unlock() { mutex_unlock(&m_mutex); }

    private:
        static mutex_t m_mutex;
    };
};

template <typename T>
bool persistent::get(const char* name, T& value)
{
    static_assert( std::is_pod_v<T> && sizeof(T) <= T6 );
    lock_guard nvm_lock;
    return _get(name, &value, sizeof(T));
}

template <typename T>
bool persistent::set(const char* name, const T& value)
{
    static_assert( std::is_pod_v<T> && sizeof(T) <= T6 );
    lock_guard nvm_lock;
    return _set(name, &value, sizeof(T))
        || _create(name, &value, sizeof(T), sizeof(T));
}
