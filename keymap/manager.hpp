#pragma once

#include <vector>               // for std::vector<>
#include "pmap.hpp"             // for pmap_t::m_pressing_slot
#include "usb_descriptor.hpp"   // for SKRO_KEYS_SIZE



namespace key {

class observer_t;



struct pressing_slot {
    pmap_t* m_slot;

    pressing_slot(pmap_t* slot): m_slot(slot) {
        m_slot->m_pressing_slot = this;
    }

    // This move constructor will deal with the reallocation of the pressing vector,
    // m_pressing_list.
    pressing_slot(pressing_slot&& other): m_slot(other.m_slot) {
        m_slot->m_pressing_slot = this;
    }

    // Note that we do not do `m_slot->m_pressing_slot = nullptr` in the destructor, which
    // will affect the moved object when its old object is deleted after the move.

    // This move assignment operator will handle the removal of middle element in the
    // vector.
    pressing_slot& operator=(pressing_slot&& other) {
        // printf("---\e[0;34m move_slot: index=%lu->%lu, deferred=%lu\e[0m\n",
        //     manager.index(&other), manager.index(this), m_index_deferred);
        m_slot = other.m_slot;
        m_slot->m_pressing_slot = this;
        return *this;
    }
};



// The key::manager_t drives all keymaps, executing on_press/release() as well as
// on_other_press/release() appropriately. It also handles the defer-presses behavior.
class manager_t {
public:
    manager_t() {
        // Mostly we would press not more than 6 keys simultaneously, but the pressing
        // vector will be expanded as needed.
        m_pressing_list.reserve(SKRO_KEYS_SIZE);
    }

    // Handle a physical press.
    void handle_press(pmap_t* slot);

    void handle_release(pmap_t* slot);

    // Execute on_press().
    void execute_press(map_t* pmap, pmap_t* slot);

    void execute_release(map_t* pmap, pmap_t* slot);

    // Start deferring all presses until their releases.
    // While we are in press-deferring mode, every press is deferred until triggered
    // later by its release or until stop_defer() is called. To preserve the order of
    // presses, however, the release triggers not only its own press but also
    // any other presses that have occurred and deferred prior to its press. For
    // example, suppose A, B, C, D and E are pressed and deferred in that order. When C
    // is released A, B and C are triggered to press when C is released. D and E are
    // left still deferred.
    void start_defer();

    // Stop deferring.
    // The start/stop_defer() can be called at any time but they takes effect when the
    // handling of current event finishes. For example, if on_other_press() calls
    // start_defer(), on_press() will be still called for the press and presses will be
    // deferred from the next press.
    void stop_defer();

    // Indicate if presses are being deferred now. Note that even if is_deferring() is
    // false, there still can exist deferred presses (i.e. is_deferred() == true), which
    // will be completed soon by complete_if_not_deferring().
    bool is_deferring() const { return m_is_deferring_presses > 0; }

    // Indicate if any key is pressing, including the deferred ones.
    bool is_any_pressing() { return !m_pressing_list.empty(); }

    // Complete any deferred presses if is_deferring() is false. It also handles the
    // possible redeferring while executing the deferred presses.
    void complete_if_not_deferring();

    // Enrolled observers will get notified of any other key presses and releases.
    // Duplicate enrollment is discarded, returning false.
    bool enroll_observer(observer_t* observer);

    // Unenroll from observer list and it will no longer get notified. Return false if
    // it is not enrolled.
    bool unenroll_observer(observer_t* observer);

private:
    std::vector<pressing_slot> m_pressing_list;

    // The start index of deferred presses in the pressing list (We use indexes instead
    // of pointers to deal with the possible reallocation of the vector.)
    size_t m_index_deferred = 0;

    unsigned m_is_deferring_presses = 0;

    // Linked list of enrolled observers.
    observer_t* m_observers = nullptr;

    // Helper method to return the index of pslot in the pressing list.
    size_t index(const pressing_slot* pslot) const {
        assert( pslot >= &m_pressing_list[0] );
        return pslot - &m_pressing_list[0];
    }

    // Add a new slot to the pressing list.
    void add_pressing_slot(pmap_t* slot);

    // Remove the given slot from the pressing list.
    void remove_pressing_slot(pmap_t* slot);

    // Indicate if press is deferred on the given slot.
    bool is_deferred(pmap_t* slot) const {
        return slot->m_pressing_slot && index(slot->m_pressing_slot) >= m_index_deferred;
    }

    // Walk through the deferred presses in the pressing list, and carry out them now,
    // up to the press on the given slot.
    inline void complete_on_release(pmap_t* slot);

    // Notify all observers, before executing press on the given slot.
    void notify_and_execute_press(pmap_t* slot);

    // Notify all observers, before executing release on the given slot.
    void notify_and_execute_release(pmap_t* slot);
};

// The global key::manager.
extern manager_t manager;

}  // namespace key
