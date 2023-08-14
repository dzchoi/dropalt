#pragma once

#include "map.hpp"

namespace key {

template <class K>
class norepeat_t: public map_t {
public: // User-facing methods
    constexpr norepeat_t(K&& key): m_key(std::forward<K>(key)) {}

private: // Methods to be called through map_t
    K m_key;

    void on_press(pmap_t* slot) {
        m_key.press(slot);
        m_key.release(slot);
    }

    void on_release(pmap_t*) {}
};

template <class K>
norepeat_t(K&&) -> norepeat_t<obj_or_ref_t<K>>;

}  // namespace key
