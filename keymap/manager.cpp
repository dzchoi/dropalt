#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "manager.hpp"
#include "map.hpp"
#include "map_proxy.hpp"
#include "observer.hpp"



namespace key {

void manager_t::execute_press(map_t* pmap, pmap_t* slot)
{
    if ( pmap->m_pressing_count++ == 0 ) {
        map_proxy_t* const pproxy = pmap->get_proxy();
        if ( pproxy == nullptr )
            pmap->on_press(slot);
        else
            pproxy->on_proxy_press(slot);
    }
}

void manager_t::notify_and_execute_press(pmap_t* slot)
{
    bool executed = false;

    for ( observer_t* observer = m_observers ; observer ; observer = observer->next )
        if ( observer->who == slot ) {
            execute_press(*slot, slot);
            executed = true;
        }
        else
            observer->on_other_press(slot);

    if ( !executed )
        execute_press(*slot, slot);
}

void manager_t::handle_press(pmap_t* slot)
{
    add_pressing_slot(slot);

    if ( !is_deferring() ) {
        DEBUG("Keymap: handle press (%p)\n", slot);
        notify_and_execute_press(slot);
    }
    else
        DEBUG("Keymap: defer press (%p)\n", slot);
}

void manager_t::execute_release(map_t* pmap, pmap_t* slot)
{
    if ( --pmap->m_pressing_count == 0 ) {
        map_proxy_t* const pproxy = pmap->get_proxy();
        if ( pproxy == nullptr )
            pmap->on_release(slot);
        else
            pproxy->on_proxy_release(slot);
    }
}

void manager_t::notify_and_execute_release(pmap_t* slot)
{
    bool executed = false;

    for ( observer_t* observer = m_observers ; observer ; observer = observer->next )
        if ( observer->who == slot ) {
            execute_release(*slot, slot);
            executed = true;
        }
        else
            observer->on_other_release(slot);

    if ( !executed )
        execute_release(*slot, slot);
}

void manager_t::handle_release(pmap_t* slot)
{
    DEBUG("Keymap: handle release (%p)\n", slot);
    if ( is_deferred(slot) )
        complete_on_release(slot);

    notify_and_execute_release(slot);

    remove_pressing_slot(slot);
}

inline void manager_t::complete_on_release(pmap_t* slot)
{
    // assert( is_deferred(slot) );
    pmap_t* ppmap;
    do {
        ppmap = m_pressing_list[m_index_deferred++].m_slot;
        DEBUG("Keymap: complete the deferred press (%p)\n", ppmap);
        notify_and_execute_press(ppmap);
        // We do not take care of start/stop_defer() here that could be possibly called
        // from notify_and_execute_press() above, as the release whose press has been has
        // been on the slot can be no longer deferred and should complete its deferred
        // press, and any other presses that have been also deferred earlier.
    } while ( ppmap != slot );
}

void manager_t::complete_if_not_deferring()
{
    while ( !is_deferring() && m_index_deferred < m_pressing_list.size() ) {
        pmap_t* const ppmap = m_pressing_list[m_index_deferred++].m_slot;
        DEBUG("Keymap: complete the deferred press (%p)\n", ppmap);
        notify_and_execute_press(ppmap);
    }
}

void manager_t::start_defer()
{
    DEBUG("Keymap: start defer\n");
    // In the usual case of calling start_defer() from on_press(), and stop_defer() from
    // on_release() or before, the deferrings do not overlap. But, we allow them to be
    // called from other places (e.g. on_other_press()) and we allow the deferrings
    // overlapping.
    m_is_deferring_presses++;
}

void manager_t::stop_defer()
{
    if ( m_is_deferring_presses > 0 )
        m_is_deferring_presses--;
    DEBUG("Keymap: stop defer\n");
}

void manager_t::add_pressing_slot(pmap_t* slot)
{
    assert( slot->m_pressing_slot == nullptr );
    if ( !is_deferring() ) {
        // When we have a new press coming in we are either press-deferring or not. If
        // not deferring we should have completed all previously deferred presses (in
        // complete_if_not_deferring() previously).
        assert( m_index_deferred == m_pressing_list.size() );
        m_index_deferred++;
    }

    m_pressing_list.emplace_back(slot);
    // DEBUG("--- add_pressing_slot: index=%lu\n", index(slot->m_pressing_slot));
}

void manager_t::remove_pressing_slot(pmap_t* slot)
{
    assert( slot->m_pressing_slot != nullptr );
    const size_t idx = index(slot->m_pressing_slot);
    if ( m_index_deferred > idx )  // i.e. if !is_deferred(slot),
        m_index_deferred--;

    slot->m_pressing_slot = nullptr;
    // DEBUG("--- remove_pressing_slot: index=%lu, deferred=%lu\n", idx, m_index_deferred);
    m_pressing_list.erase(m_pressing_list.begin() + idx);
}

bool manager_t::enroll_observer(observer_t* observer)
{
    observer_t** pnext = &m_observers;
    for ( ; *pnext ; pnext = &(*pnext)->next )
        if ( *pnext == observer )
            return false;

    // Append it to the end of the list. The enroll_observer() can be executed, not only
    // from on_press/release(), but also from on_other_press/release(). In that case
    // on_other_press/release() of the newly enrolled observer will be called immediately
    // after.
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
            // unenroll_observer() from on_other_press/release() while iterating through
            // the observer list.
            return true;
        }
    return false;
}

manager_t manager;

}  // namespace key
