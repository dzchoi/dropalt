#include "log.h"
#include "seeprom.h"            // for seeprom_*()

#include "persistent.hpp"



mutex_t persistent::lock_guard::m_mutex = MUTEX_INIT;

const size_t persistent::m_size = seeprom_size();

uint16_t& persistent::nvm_occupied = *static_cast<uint16_t*>(seeprom_addr(0));

void persistent::init()
{
    seeprom_init();
    seeprom_unbuffered();  // Enable unbuffered mode.

    LOG_DEBUG("seeprom: seeprom_size=%d\n", m_size);
    if ( nvm_occupied > m_size )  // E.g. nvm_occupied == 0xffff
        nvm_occupied = sizeof(nvm_occupied);
}

void* persistent::create(const char* name, size_t value_size)
{
    const size_t name_size = __builtin_strlen(name) + 1;  // including '\0'.
    const size_t new_size = 1 + name_size + value_size;
    if ( new_size > 0xff || nvm_occupied + new_size > m_size )
        return nullptr;  // Entry size exceeds 255 bytes or causes SEEPROM overflow.

    uint8_t* here = end();
    *here++ = new_size;
    __builtin_memcpy(here, name, name_size);
    nvm_occupied += new_size;
    LOG_DEBUG("seeprom: create \"%s\"\n", name);
    // commit_later();  // will be performed by get()/set() outside.
    return here + name_size;  // Return the value-field address.
}

std::pair<void*, size_t> persistent::find(const char* name)
{
    uint8_t* it = begin();
    uint8_t* const end = persistent::end();
    while ( it < end ) {
        char* const pname = (char*)it + 1;
        it += *it;  // Move to the next entry.
        if ( __builtin_strcmp(name, pname) == 0 ) {
            void* pvalue = pname + __builtin_strlen(name) + 1;
            return { pvalue, it - (uint8_t*)pvalue };
        }
    }
    return { nullptr, 0 };
}

const char* persistent::next(const char* name)
{
    lock_guard seeprom_lock;
    uint8_t* it = begin();
    uint8_t* const end = persistent::end();
    if ( name ) {
        auto [pvalue, found_size] = find(name);
        it = pvalue ? (uint8_t*)pvalue + found_size : end;
    }

    // Bypass entries marked as deleted.
    while ( it < end && it[1] == 0xff )
        it += *it;
    return it < end ? (char*)it + 1 : nullptr;
}

bool persistent::get(const char* name, void* value, size_t value_size)
{
    lock_guard seeprom_lock;
    auto [pvalue, found_size] = find(name);
    if ( pvalue == nullptr )
        return false;
    if ( value_size > found_size )
        value_size = found_size;

    __builtin_memcpy(value, pvalue, value_size);
    return true;
}

bool persistent::set(const char* name, const void* value, size_t value_size)
{
    lock_guard seeprom_lock;
    auto [pvalue, found_size] = find(name);
    if ( pvalue == nullptr ) {
        pvalue = create(name, value_size);
        if ( pvalue == nullptr )
            return false;
    }
    else if ( value_size > found_size )
        value_size = found_size;

    __builtin_memcpy(pvalue, value, value_size);
    commit_later();
    return true;
}

bool persistent::remove(const char* name)
{
    lock_guard seeprom_lock;
    uint8_t* here = (uint8_t*)find(name).first;
    if ( here == nullptr )
        return false;
    here -= __builtin_strlen(name) + 1;
    *here = 0xff;  // Clear the first character of the name.

    // Tail reclamation
    uint8_t* tail = begin();
    uint8_t* const end = persistent::end();
    for ( uint8_t* it = begin() ; it < end ; ) {
        uint8_t name0 = it[1];
        it += *it;
        if ( name0 != 0xff )
            tail = it;
    }
    nvm_occupied = tail - static_cast<uint8_t*>(seeprom_addr(0));

    commit_now();
    return true;
}

void persistent::remove_all()
{
    lock_guard seeprom_lock;
    __builtin_memset(&nvm_occupied, 0xff, m_size);  // Clear the flash.
    nvm_occupied = sizeof(nvm_occupied);
    commit_now();
}
