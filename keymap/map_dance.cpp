#include "map_dance.hpp"



namespace key {

void map_dance_t::on_proxy_press(pmap_t* slot)
{
    if ( m_step++ == 0 ) {
        assert( m_is_finished );
        m_is_finished = false;  // Start the dance.
        start_observe(slot);
    }

    start_timer(slot);  // (Re)start the timer.
    on_press(slot);
}

void map_dance_t::on_proxy_release(pmap_t* slot)
{
    if ( m_is_finished ) {
        on_release(slot);
        m_step = 0;
    }
}

void map_dance_t::on_timeout(pmap_t* slot)
{
    stop_observe();
    m_is_finished = true;  // Finish the dance.
    on_finish(slot);
    if ( !is_pressed() ) {
        on_release(slot);  // Notify of the release late.
        m_step = 0;
    }
}

void map_dance_t::on_other_press(pmap_t* slot)
{
    stop_timer();
    on_timeout(slot);
}

void map_dance_t::finish()
{
    // Will skip calling on_finish().
    stop_timer();
    stop_observe();
    m_is_finished = true;
}

}  // namespace key
