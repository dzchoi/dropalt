#include "assert.h"             // for assert()

#include "pmap.hpp"



namespace key {

lamp_id_t pmap_t::lamp_id() const
{
    return m_pmap->get_lamp() ? m_pmap->get_lamp()->lamp_id() : NO_LAMP;
}

ohsv_t pmap_t::is_lamp_lit() const
{
    return m_pmap->get_lamp() ? m_pmap->get_lamp()->is_lamp_lit() : ohsv_t();
}

void pmap_t::lamp_on_off(bool on_off)
{
    assert( m_pmap->get_lamp() );
    if ( on_off )
        m_pmap->get_lamp()->lamp_on(this);
    else
        m_pmap->get_lamp()->lamp_off(this);
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
