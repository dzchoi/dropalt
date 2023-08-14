#include "map.hpp"
#include "map_proxy.hpp"        // for map_proxy_t, on_proxy_press/release()



namespace key {

void map_t::press(pmap_t* slot)
{
    if ( m_press_count++ == 0 ) {
        map_proxy_t* const pproxy = get_proxy();
        if ( pproxy )
            pproxy->on_proxy_press(slot);
        else
            on_press(slot);
    }
}

void map_t::release(pmap_t* slot)
{
    if ( --m_press_count == 0 ) {
        map_proxy_t* const pproxy = get_proxy();
        if ( pproxy )
            pproxy->on_proxy_release(slot);
        else
            on_release(slot);
    }
}

}  // namespace key
