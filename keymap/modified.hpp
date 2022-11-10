#pragma once

#include "map_modified.hpp"



namespace key {

class modified_t: public map_modified_t {
public: // User-facing methods
    constexpr modified_t(map_t& unmodified, map_t& modified, const map_t& modifier)
    : map_modified_t(modifier), m_unmodified(unmodified), m_modified(modified) {}

private: // Methods to be called by map_modified_t
    void on_press(pmap_t* slot) { m_unmodified.press(slot); }

    void on_release(pmap_t* slot) { m_unmodified.release(slot); }

    void on_modified_press(pmap_t* slot) { m_modified.press(slot); }

    void on_modified_release(pmap_t* slot) { m_modified.release(slot); }

    map_t& m_unmodified;
    map_t& m_modified;
};

}  // namespace key
