#include "map_modified.hpp"



namespace key {

void map_modified_t::on_proxy_press(pmap_t* slot)
{
    assert( m_modified == false );
    if ( m_modifier.is_pressing() ) {
        m_modified = true;
        on_modified_press(slot);
    } else
        on_press(slot);
}

void map_modified_t::on_proxy_release(pmap_t* slot)
{
    if ( m_modified ) {
        on_modified_release(slot);
        m_modified = false;
    } else
        on_release(slot);
}

}  // namespace key
