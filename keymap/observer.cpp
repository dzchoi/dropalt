#include "observer.hpp"



namespace key {

bool _any_t::enroll(observer_t* observer)
{
    if ( observer->next == nullptr && m_observers != observer ) {
        observer->next = m_observers;
        m_observers = observer;
        return true;
    }
    return false;
}

bool _any_t::unenroll(observer_t* observer)
{
    for ( observer_t** pnext = &m_observers ; *pnext ; pnext = &(*pnext)->next )
        if ( *pnext == observer ) {
            *pnext = observer->next;
            observer->next = nullptr;
            return true;
        }
    return false;
}

void _any_t::notify_of_press(pmap_t* ppmap)
{
    for ( observer_t* observer = m_observers ; observer ; observer = observer->next )
        observer->on_other_press(ppmap);
}

void _any_t::notify_of_release(pmap_t* ppmap)
{
    for ( observer_t* observer = m_observers ; observer ; observer = observer->next )
        observer->on_other_release(ppmap);
}

}  // namespace key
