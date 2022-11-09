#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "map.hpp"
#include "observer.hpp"
#include "manager.hpp"



namespace key {

void manager_t::notify_of_press(pmap_t* slot)
{
    for ( observer_t* observer = m_observers ; observer ; observer = observer->next )
        observer->on_other_press(slot);
}

void manager_t::execute_press(map_t* pmap, pmap_t* slot)
{
    if ( pmap->m_pressing_count++ == 0 )
        pmap->on_press(slot);
}

void manager_t::handle_press(pmap_t* slot)
{
    add_slot(slot);

    if ( is_deferring() ) {
        DEBUG("Keymap: defer press (%p)\n", slot);
        return;
    }

    notify_of_press(slot);
    execute_press(*slot, slot);
}

void manager_t::notify_of_release(pmap_t* slot)
{
    for ( observer_t* observer = m_observers ; observer ; observer = observer->next )
        observer->on_other_release(slot);
}

void manager_t::execute_release(map_t* pmap, pmap_t* slot)
{
    if ( pmap->m_pressing_count > 0 && --pmap->m_pressing_count == 0 )
        pmap->on_release(slot);
}

void manager_t::handle_release(pmap_t* slot)
{
    if ( is_deferring(slot) )
        complete_deferred(slot);

    execute_release(*slot, slot);
    notify_of_release(slot);

    remove_slot(slot);
}

void manager_t::complete_deferred(pmap_t* slot)
{
    assert( is_deferring() );
    while ( m_index_deferred < int(m_pressing_slots.size()) ) {
        pmap_t* const ppmap = m_pressing_slots[m_index_deferred++].m_slot;
        DEBUG("Keymap: complete the deferred press (%p)\n", ppmap);
        notify_of_press(ppmap);
        execute_press(*ppmap, ppmap);
        if ( ppmap == slot )
            break;
    }
}

void manager_t::remove_slot(pmap_t* slot)
{
    assert( slot->m_pressing != nullptr );
    const int idx = index(slot->m_pressing);
    if ( m_index_deferred > idx )  // i.e. if !is_deferring(slot),
        m_index_deferred--;
    slot->m_pressing = nullptr;
    // printf("--- remove_slot: index=%d, deferred=%d\n", idx, m_index_deferred);
    m_pressing_slots.erase(m_pressing_slots.begin() + idx);
}

bool manager_t::is_deferring(pmap_t* slot) const
{
    return is_deferring() && index(slot->m_pressing) >= m_index_deferred;
}

bool manager_t::enroll_observer(observer_t* observer)
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

bool manager_t::unenroll_observer(observer_t* observer)
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

manager_t manager;

}  // namespace key
