#include "assert.h"             // for assert()

#include "lamp_id.hpp"          // for lamp_id_t, ::is_lamp_lit()
#include "map_lamp.hpp"         // for map_lamp_t, lamp_id(), is_lamp_lit()
#include "map.hpp"              // for get_lamp()
#include "pmap.hpp"



namespace key {

lamp_id_t pmap_t::lamp_id() const
{
    const map_lamp_t* const plamp = m_pmap->get_lamp();
    return plamp ? plamp->lamp_id() : NO_LAMP;
}

ohsv_t pmap_t::is_lamp_lit() const
{
    const map_lamp_t* const plamp = m_pmap->get_lamp();
    return plamp ? plamp->is_lamp_lit() : ohsv_t();
}

void pmap_t::refresh_lamp()
{
    map_lamp_t* const plamp = m_pmap->get_lamp();
    assert( plamp );
    ::is_lamp_lit(lamp_id()) ? plamp->lamp_on(this) : plamp->lamp_off(this);
}

}  // namespace key



led_iter& led_iter::advance_if_invalid()
{
    while ( m_slot < &key::maps[NUM_SLOTS] && m_slot->led_id() == NO_LED )
        m_slot++;
    return *this;
}

lamp_iter& lamp_iter::advance_if_invalid()
{
    while ( m_slot < &key::maps[NUM_SLOTS] && m_slot->lamp_id() == NO_LAMP )
        m_slot++;
    return *this;
}
