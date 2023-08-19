#pragma once

#include <iterator>             // for std::input_iterator_tag
#include "color.hpp"            // for hsv_t, ohsv_t
#include "features.hpp"         // for RGB_INDICATOR_COLOR
#include "lamp_id.hpp"          // for lamp_id_t



namespace key {
class pmap_t;
}



// Simple led indicator with a solid color.

// `lamp_t` creates an indicator lamp instance, to which `pmap_t` can assign its sitting
// slot. `when_lamp_on()` or `when_lamp_off()` will be called after the indicator lamp
// is turned on or off, which does nothing by default but can be overriden by a child
// class derived from lamp_t.
class lamp_t {
public:
    constexpr lamp_t(lamp_id_t lamp_id, hsv_t hsv =RGB_INDICATOR_COLOR)
    : m_hsv(hsv), m_lamp_id(lamp_id) {}

    // Not copyable nor movable.
    lamp_t(const lamp_t&) =delete;
    lamp_t& operator=(const lamp_t&) =delete;

    lamp_id_t lamp_id() const { return m_lamp_id; }

    // Return `oshv_t(m_hsv)` if the lamp is on, or `ohsv_t()` otherwise.
    ohsv_t is_lit() const;

    // Return the slot associated with this lamp.
    key::pmap_t* slot() const { return m_slot; }

    // Execute when_lamp_on/off() according to the current lamp state.
    void execute_lamp();

private:
    const hsv_t m_hsv;
    const lamp_id_t m_lamp_id;

    friend class key::pmap_t;  // for m_begin, m_next and m_slot
    friend class lamp_iter;  // for m_begin and m_next

    virtual void when_lamp_on(key::pmap_t*) {}
    virtual void when_lamp_off(key::pmap_t*) {}

    // Header of the linked list.
    static inline lamp_t* m_begin = nullptr;

    lamp_t* m_next = nullptr;

    key::pmap_t* m_slot = nullptr;
};



class lamp_iter {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = lamp_t;
    using pointer = value_type*;
    using reference = value_type&;

    lamp_iter(pointer plamp): m_ptr(plamp) {}

    reference operator*() const { return *m_ptr; }
    pointer operator->() const { return m_ptr; }

    lamp_iter& operator++() { m_ptr = m_ptr->m_next; return *this; }  // prefix
    lamp_iter operator++(int);  // postfix

    friend bool operator==(const lamp_iter& a, const lamp_iter& b) {
        return a.m_ptr == b.m_ptr;
    }

    friend bool operator!=(const lamp_iter& a, const lamp_iter& b) {
        return !(a == b);
    }

    static lamp_iter begin() { return lamp_iter(lamp_t::m_begin); }

    static lamp_iter end() { return lamp_iter(nullptr); }

private:
    pointer m_ptr;
};
