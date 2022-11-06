#include "keymap.hpp"
#include "observer.hpp"
#include "whole.hpp"



namespace key {

void whole_t::on_press(pmap_t* ppmap)
{
    // Notify all observers of the press.
    for ( observer_t* observer = m_observers ; observer ; observer = observer->next )
        observer->on_other_press(ppmap);

    (*ppmap)->on_press(ppmap);
}

void whole_t::on_release(pmap_t* ppmap)
{
    (*ppmap)->on_release(ppmap);

    // Notify all observers of the release.
    for ( observer_t* observer = m_observers ; observer ; observer = observer->next )
        observer->on_other_release(ppmap);
}

bool whole_t::enroll_observer(observer_t* observer)
{
    observer_t** pnext = &m_observers;
    for ( ; *pnext ; pnext = &(*pnext)->next )
        if ( *pnext == observer )
            return false;

    // Append it to the end of the list. The enroll_observer() can be executed from
    // on_other_press/release() as well as from on_press/release(). Then the newly
    // enrolled observer's on_other_press/release() will be called immediately after.
    observer->next = nullptr;
    *pnext = observer;
    return true;
}

bool whole_t::unenroll_observer(observer_t* observer)
{
    for ( observer_t** pnext = &m_observers ; *pnext ; pnext = &(*pnext)->next )
        if ( *pnext == observer ) {
            *pnext = observer->next;
            // We do not clear observer->next here in order to be able to execute
            // unenroll_observer() from on_other_press/release().
            return true;
        }
    return false;
}

}  // namespace key
