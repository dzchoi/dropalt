#include "assert.h"

#define ENABLE_DEBUG 1
#include "debug.h"

#include "persistent.hpp"



persistent::persistent()
{
    assert( sizeof(persistent) <= seeprom_size() );
    seeprom_init();

    const uint32_t _magic_number = magic_number;
    const uint8_t _major_version = major_version;
    read(&persistent::magic_number);
    read(&persistent::major_version);

    if ( magic_number == _magic_number && major_version == _major_version )
        read_all();
    else {
        magic_number = _magic_number;
        major_version = _major_version;
        write_all();
        DEBUG("persistent: reset default values\n");
    }
}
