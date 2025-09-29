#include "log.h"
#include "seeprom.h"            // for seeprom_*()
#include "usb2422.h"            // for USB_PORT_1

#include "persistent.hpp"



uint8_t (&persistent::nvm)[SEEPROM_SIZE] = *(uint8_t (*)[SEEPROM_SIZE])(SEEPROM_ADDR);

mutex_t persistent::lock_guard::m_mutex = MUTEX_INIT;

void persistent::init()
{
    seeprom_init();
    seeprom_unbuffered();  // Enable unbuffered mode.

    if ( begin()->uint8 == 0xff ) {  // If not initialized,
        begin()->size2 = SEEPROM_SIZE2;  // == log2(SEEPROM_SIZE)
        // Note: Bootloader requires `last_host_port` to be the first entry in NVM.
        persistent::set("last_host_port", USB_PORT_1);
    }
}

bool persistent::_create(const char* name, const void* value, size_t value_size, uint8_t value_type)
{
    const size_t name_size = __builtin_strlen(name) + 1;  // including '\0'.
    const size_t new_size = 1 + name_size + value_size;
    const uint8_t new_size2 = (sizeof(size_t) * CHAR_BIT) - __builtin_clz(new_size - 1);

    // Maximum SEEPROM_SIZE is limited to 16KB (= 2^14).
    static_assert( SEEPROM_SIZE2 < 15 );
    block_header no_block(SEEPROM_SIZE2 + 1, TFREE, false);  // .free = false
    iterator found(&no_block);

    // Find the smallest block that the requested size can fit in.
    for ( auto it = begin() ; it != end() ; ++it )
        if ( it->free ) {
            if ( it->size2 == new_size2 ) {
                found = it;
                break;
            }
            if ( new_size2 < it->size2 && it->size2 < found->size2 )
                found = it;
        }
        else if ( __builtin_strcmp(name, it.to_name()) == 0 )
            return false;  // Already existing.

    // Return false if not found.
    if ( !found->free )
        return false;

    // Split the block if its size is larger than required.
    for ( uint8_t blk_size2 = found->size2 ; blk_size2 > new_size2 ; ) {
        blk_size2--;
        *found.buddy(1 << blk_size2) = block_header(blk_size2);
    }

    // Allocate the block.
    *found = block_header(new_size2, value_type, false);

    // Populate the name field and value field
    __builtin_memcpy(found.to_name(), name, name_size);
    __builtin_memcpy(found.to_name() + name_size, value, value_size);
    // LOG_DEBUG("seeprom: _create \"%s\" type=%d\n", name, value_type);
    // _commit_later();  // will be performed by get()/set() outside.
    return true;
}

bool persistent::_remove(iterator it)
{
    if ( it == end() || it->free )
        return false;

    // Merge the buddy blocks if they are free.
    uint8_t blk_size2 = it->size2;
    for ( ; blk_size2 < SEEPROM_SIZE2 ; blk_size2++ ) {
        auto buddy = it.buddy(1 << blk_size2);
        if ( !(buddy->free && buddy->size2 == blk_size2) )
            break;
        // If the buddy block is free, do not change it but move up to the parent block.
        it = it.parent(1 << blk_size2);
    }

    // Make this block free.
    *it = block_header(blk_size2);
    // LOG_DEBUG("seeprom: _remove \"%s\"\n", it.to_name());
    _commit_later();
    return true;
}

persistent::iterator persistent::_find(const char* name)
{
    if ( name )
        for ( auto it = begin() ; it != end() ; ++it ) {
            if ( !it->free && __builtin_strcmp(name, it.to_name()) == 0 )
                return it;
        }

    return end();
}

const char* persistent::next(const char* name)
{
    lock_guard nvm_lock;
    iterator it = begin();
    if ( name ) {
        it = _find(name);
        if ( it != end() )
            ++it;
    }

    while ( it != end() && it->free )
        ++it;

    return it != end() ? it.to_name() : nullptr;
}

bool persistent::_get(const char* name, void* value, size_t value_size)
{
    iterator it = _find(name);
    if ( it == end() )
        return false;
    if ( value_size > it->value_type )
        // Note that value_size would be 4 if value_type = TFLOAT (=7).
        value_size = it->value_type;

    __builtin_memcpy(value, it.to_value(), value_size);
    return true;
}

const char* persistent::get(const char* name)
{
    lock_guard nvm_lock;
    return (const char*)_find(name).to_value();
}

bool persistent::_set(const char* name, const void* value, size_t value_size)
{
    iterator it = _find(name);
    if ( it == end() )
        return false;
    if ( value_size > it->value_type )
        value_size = it->value_type;

    __builtin_memcpy(it.to_value(), value, value_size);
    _commit_later();
    return true;
}

bool persistent::set(const char* name, float value)
{
    lock_guard nvm_lock;
    return _set(name, &value, sizeof(float))
        || _create(name, &value, sizeof(float), TFLOAT);
}

bool persistent::set(const char* name, const char* value)
{
    lock_guard nvm_lock;
    auto it = _find(name);
    _remove(it);
    return _create(name, value, __builtin_strlen(value) + 1, TSTRING);
}

void persistent::remove_all()
{
    lock_guard nvm_lock;
    // Clearing the entire SEEPROM may take a long time, potentially triggering a
    // watchdog reset.
    // __builtin_memset(begin(), 0xff, sizeof(nvm));  // Clear the SEEPROM.
    *begin() = block_header(SEEPROM_SIZE2);
    commit_now();
}

size_t persistent::size()
{
    lock_guard nvm_lock;
    size_t count = 0;
    for ( auto it = begin() ; it != end() ; ++it ) {
        // LOG_DEBUG("seeprom: %p size=%d type=%d free=%d\n",
        //     it.operator->(), 1 << it->size2, it->value_type, it->free);
        if ( !it->free )
            count++;
    }
    return count;
}
