#pragma once

#include "map.hpp"



namespace key {

class map_proxy_t: public map_t {
protected: // Methods to be used by child classes
    constexpr map_proxy_t() =default;

    // Allow access to these methods to child classes.
    using map_t::on_press;
    using map_t::on_release;

private: // Methods to be called by key::manager
    friend class manager_t;

    map_proxy_t* get_proxy() { return this; }

    virtual void on_proxy_press(pmap_t* slot) =0;
    virtual void on_proxy_release(pmap_t* slot) =0;
};

}  // namespace key
