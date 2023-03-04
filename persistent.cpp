#include "assert.h"

#define ENABLE_DEBUG 1
#include "debug.h"

#include "persistent.hpp"



persistent::persistent()
{
    assert( sizeof(persistent) <= seeprom_size() );
    seeprom_init();

    const uint32_t _magic_number = magic_number;
    const uint16_t _layout_version = layout_version;
    read(&persistent::magic_number);
    read(&persistent::layout_version);

    if ( magic_number == _magic_number && layout_version == _layout_version )
        read_all();
    else {
        // Reset the NVM if it is corrupted or old-versioned.
        magic_number = _magic_number;
        layout_version = _layout_version;
        write_all();
        DEBUG("persistent: reset default values\n");
    }
}
