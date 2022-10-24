#include "observer.hpp"



namespace key {

bool _any_t::enroll(observer_t* observer)
{
    observer_t** pnext = &m_observers;
    for ( ; *pnext ; pnext = &(*pnext)->next )
        if ( *pnext == observer )
            return false;
    // Append it to the end of the list. Its own on_other_press/release() will be also
    // called when enroll() is performed from on_other_press/release().
    observer->next = nullptr;
    *pnext = observer;
    return true;
}

bool _any_t::unenroll(observer_t* observer)
{
    for ( observer_t** pnext = &m_observers ; *pnext ; pnext = &(*pnext)->next )
        if ( *pnext == observer ) {
            *pnext = observer->next;
            // We do not clear here observer->next to be able to perform unenroll() from
            // on_other_press/release().
            return true;
        }
    return false;
}

void _any_t::notify_of_press(pbase_t* ppbase)
{
    for ( observer_t* observer = m_observers ; observer ; observer = observer->next )
        observer->on_other_press(ppbase);
}

void _any_t::notify_of_release(pbase_t* ppbase)
{
    for ( observer_t* observer = m_observers ; observer ; observer = observer->next )
        observer->on_other_release(ppbase);
}

}  // namespace key
