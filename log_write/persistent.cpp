#include "assert.h"
#include "log.h"

#include <algorithm>            // for std::min()
#include <cstdio>               // for vsnprintf()
#include "persistent.hpp"



// As being constexpr, this is allocated in .text section.
constexpr persistent persistent::m_default;

void persistent::init()
{
    seeprom_init();
    assert( sizeof(persistent) <= seeprom_size() );

    if ( obj().magic_number != m_default.magic_number
      || obj().layout_version != m_default.layout_version )
    {
        // Reset NVM if it is corrupted or out-dated.
        reset_all();
        LOG_DEBUG("persistent: reset default values\n");
    }
}

void persistent::add_log(const char* format, va_list vlist)
{
    const uint16_t seeprom_size = ::seeprom_size();
    uint16_t occupied;
    read(&persistent::occupied, &occupied);

    if ( occupied >= seeprom_size )
        return;

    // If occupied < seeprom_size vsnprintf() fills in the buffer up to (seeprom_size - 1)
    // characters, but returns the total number of characters (not including '\0') that
    // would have been written if the size limit was not imposed.
    int len = vsnprintf(static_cast<char*>(seeprom_addr(occupied)),
        seeprom_size - occupied, format, vlist);

    if ( len > 0 )
        obj().occupied = std::min(seeprom_size + 0, occupied + len);
}

void persistent::reset_all()
{
    seeprom_buffered();
    seeprom_write(0, &m_default, sizeof(persistent));
    seeprom_flush();
}
