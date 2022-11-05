#pragma once

#include <vector>               // for std::vector<>
#include "keymap.hpp"



namespace key {

class pressing_list {
public:
// Methods to work on the pressing list as a whole.

    static void reserve(size_t n) { m_press_vector.reserve(n); }

    // Add ppmap to the pressing list.
    static void add(pmap_t* ppmap) {
        m_press_vector.emplace_back(ppmap);
        // printf("--- add: index=%d\n", (*ppmap)->m_pressing->index());
    }

    // Remove ppmap from the pressing list.
    static void remove(pmap_t* ppmap) {
        assert( (*ppmap)->m_pressing != nullptr );
        const int idx = (*ppmap)->m_pressing->index();
        if ( m_index_deferred > idx )  // i.e. if !is_deferring(ppmap),
            m_index_deferred--;
        (*ppmap)->m_pressing = nullptr;
        // printf("--- remove: index=%d, deferred=%d\n", idx, m_index_deferred);
        m_press_vector.erase(m_press_vector.begin() + idx);
    }

    // Start deferring all presses until their release.
    static void start_defering() {
        assert( !is_deferring() );
        m_index_deferred = m_press_vector.size();
    }

    // Stop deferring (only if no presses are deferred).
    static void stop_defering() {
        assert( m_index_deferred == int(m_press_vector.size()) );
        m_index_deferred = -1;
    }

    // Indicate if presses are being deferred until their release.
    static bool is_deferring() { return m_index_deferred >= 0; }

    // Indicate if the given ppmap's press is deferred.
    static bool is_deferring(pmap_t* ppmap) {
        return is_deferring() && (*ppmap)->m_pressing->index() >= m_index_deferred;
    }

    // Walk through the deferred presses collected in the pressing list, and carry out
    // the presses now (up to ppmap, or all if ppmap == nullptr).
    template <typename F>
    static void complete_deferred(F execute_press, pmap_t* ppmap =nullptr) {
        assert( is_deferring() );
        while ( m_index_deferred < int(m_press_vector.size()) ) {
            pmap_t* const _ppmap = m_press_vector[m_index_deferred++].m_ppmap;
            execute_press(_ppmap);
            if ( _ppmap == ppmap )
                break;
        }
    }

// Methods to work on each member in the pressing list.

    pressing_list(pmap_t* ppmap): m_ppmap(ppmap) {
        (*m_ppmap)->m_pressing = this;
    }

    // This move constructor will deal with the reallocation of the vector.
    pressing_list(pressing_list&& other): m_ppmap(other.m_ppmap) {
        (*m_ppmap)->m_pressing = this;
    }

    // Note that we do not do `(*m_ppmap)->m_pressing = nullptr` in the destructor, which
    // will affect the moved object when its old object is deleted after the move.

    // This move assignment operator will handle the removal of middle element in the
    // vector.
    pressing_list& operator=(pressing_list&& other) {
        // printf("---\e[0;34m assign: index=%d->%d, deferred=%d\e[0m\n",
        //     other.index(), index(), m_index_deferred);
        m_ppmap = other.m_ppmap;
        (*m_ppmap)->m_pressing = this;
        return *this;
    }

private:
    pmap_t* m_ppmap;

    static inline std::vector<pressing_list> m_press_vector;

    // The start index of deferred presses in m_press_vector. -1 indicates not deferring
    // presses. (We use indexes instead of pointers to handle the possible reallocation
    // of the vector.)
    static inline int m_index_deferred = -1;

    // Helper method to return the index of `this` object in m_index_deferred.
    int index() const { return this - &m_press_vector[0]; }
};

}
