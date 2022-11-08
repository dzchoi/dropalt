#pragma once

#include <vector>               // for std::vector<>
#include "pmap.hpp"             // for pmap_t::m_pressing
#include "usb_descriptor.hpp"   // for SKRO_KEYS_SIZE



namespace key {

class observer_t;



struct pressing_slot {
    pmap_t* m_slot;

    pressing_slot(pmap_t* slot): m_slot(slot) {
        m_slot->m_pressing = this;
    }

    // This move constructor will deal with the reallocation of the pressing vector,
    // m_pressing_slots.
    pressing_slot(pressing_slot&& other): m_slot(other.m_slot) {
        m_slot->m_pressing = this;
    }

    // Note that we do not do `m_slot->m_pressing = nullptr` in the destructor, which
    // will affect the moved object when its old object is deleted after the move.

    // This move assignment operator will handle the removal of middle element in the
    // vector.
    pressing_slot& operator=(pressing_slot&& other) {
        // printf("---\e[0;34m move_slot: index=%d->%d, deferred=%d\e[0m\n",
        //     whole.index(&other), whole.index(this), m_index_deferred);
        m_slot = other.m_slot;
        m_slot->m_pressing = this;
        return *this;
    }
};



// The key::whole_t represents and handles all keys. It executes on_press/release() of
// each key and also executes on_other_press/release() for observers.
class whole_t {
public:
    whole_t() {
        // Mostly we would press not more than 6 keys simultaneously, but the pressing
        // vector will be expanded as needed.
        m_pressing_slots.reserve(SKRO_KEYS_SIZE);
    }

    void handle_press(pmap_t* slot);

    void handle_release(pmap_t* slot);

    void execute_press(map_t* pmap, pmap_t* slot);

    void execute_release(map_t* pmap, pmap_t* slot);

    // Start deferring all presses until their releases.
    void start_defering() {
        assert( !is_deferring() );
        m_index_deferred = m_pressing_slots.size();
    }

    // Stop deferring (only if no presses are deferred).
    void stop_defering() {
        assert( m_index_deferred == int(m_pressing_slots.size()) );
        m_index_deferred = -1;
    }

    // Indicate if any key is pressing, including the deferred ones.
    bool is_any_pressing() { return !m_pressing_slots.empty(); }

    // Walk through the deferred presses collected in the pressing vector, and carry out
    // the presses now (up to slot, or all if slot == nullptr).
    void complete_deferred(pmap_t* slot =nullptr);

    // Enrolled observers will get notified of any other key presses and releases.
    // Duplicate enrollment is discarded, returning false.
    bool enroll_observer(observer_t* observer);

    // Unenroll from observer list and it will no longer get notified. Return false if
    // it is not enrolled.
    bool unenroll_observer(observer_t* observer);

private:
    std::vector<pressing_slot> m_pressing_slots;

    // The start index of deferred presses in the pressing vector; -1 indicates not
    // deferring presses. (We use indexes instead of pointers to handle the possible
    // reallocation of the vector.)
    int m_index_deferred = -1;

    // Linked list of enrolled observers.
    observer_t* m_observers = nullptr;

    // Helper method to return the index of pslot in m_pressing_slots.
    int index(const pressing_slot* pslot) const { return pslot - &m_pressing_slots[0]; }

    // Add slot to the pressing list.
    void add_slot(pmap_t* slot) {
        m_pressing_slots.emplace_back(slot);
        // printf("--- add_slot: index=%d\n", index(slot->m_pressing));
    }

    // Remove slot from the pressing list.
    void remove_slot(pmap_t* slot);

    // Indicate if presses are being deferred.
    bool is_deferring() const { return m_index_deferred >= 0; }

    // Indicate if the given slot's press is deferred.
    bool is_deferring(pmap_t* slot) const;
};

// The global key::whole.
extern whole_t whole;

}  // namespace key
