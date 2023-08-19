#include <utility>              // for std::move()
#include "lamp.hpp"             // for lamp_t, lamp_iter, slot(), is_lit(), ...
#include "lamp_id.hpp"          // for ::is_lamp_lit()
#include "pmap.hpp"



namespace key {

pmap_t::pmap_t(map_t& map, lamp_t& lamp)
: pmap_t(map)
{
    // Link lamps in ascending order of their m_slot.
    lamp_t** pplamp = &lamp_t::m_begin;
    while ( *pplamp && (*pplamp)->m_slot < this )
        pplamp = &(*pplamp)->m_next;

    lamp.m_next = *pplamp;
    *pplamp = &lamp;
    lamp.m_slot = this;
}

ohsv_t pmap_t::is_lamp_lit() const
{
    for ( auto it = lamp_iter::begin() ; it != lamp_iter::end() ; ++it )
        if ( it->slot() == this )
            return it->is_lit();
    return {};
}

}  // namespace key



led_iter led_iter::begin()
{
    return std::move(
        led_iter(&key::maps[0], lamp_iter::begin()).advance_if_invalid() );
}

led_iter& led_iter::advance_if_invalid()
{
    if ( m_pslot != &key::maps[NUM_SLOTS] ) {
        // If slot has an indicator lamp lit we skip over it.
        if ( m_pslot == m_plamp->slot() ) {
            if ( is_lamp_lit(m_plamp++->lamp_id()) ) {
                ++m_pslot;
                return advance_if_invalid();  // tail-call optimization assumed
            }
        }

        else if ( m_pslot->led_id() == NO_LED ) {
            ++m_pslot;
            return advance_if_invalid();
        }
    }

    return *this;
}
