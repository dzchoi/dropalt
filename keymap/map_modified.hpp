#pragma once

#include "map_proxy.hpp"



namespace key {

class map_modified_t: public map_proxy_t {
protected: // Methods to be used by child classes
    constexpr map_modified_t(map_t& modifier): m_modifier(modifier) {}

    map_t& m_modifier;

private: // Methods to be called by key::manager
    void on_proxy_press(pmap_t* slot);

    void on_proxy_release(pmap_t* slot);

    virtual void on_modified_press(pmap_t* slot) =0;

    virtual void on_modified_release(pmap_t* slot) =0;

    bool m_is_modified = false;
};

}  // namespace key
