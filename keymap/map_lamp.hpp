#pragma once

#include "color.hpp"            // for hsv_t, ohsv_t
#include "features.hpp"         // for RGB_INDICATOR_COLOR
#include "lamp_id.hpp"          // for lamp_id_t
#include "map.hpp"              // for base class



namespace key {

// Simple led indicator with a solid color.

class map_lamp_t: public map_t {
public:
    constexpr map_lamp_t(lamp_id_t lamp, hsv_t hsv)
    : m_hsv(hsv), m_lamp(lamp) {}

    lamp_id_t lamp_id() const { return m_lamp; }

    ohsv_t is_lamp_lit() const {
        return ::is_lamp_lit(m_lamp) ? ohsv_t(m_hsv) : ohsv_t();
    }

private:
    const hsv_t m_hsv;
    const lamp_id_t m_lamp;

    map_lamp_t* get_lamp() { return this; }

    friend class pmap_t;  // for lamp_on() and lamp_off()

    virtual void lamp_on(pmap_t*) {}
    virtual void lamp_off(pmap_t*) {}
};



// `lamp_t` associates the led of a keymap's sitting slot with an indicator lamp.
// E.g. `lamp_t(CAPS_LOCK_LAMP, CAPSLOCK)` will define a new keymap, whose led is
// associated with with the indicator lamp for CAPS_LOCK_LAMP when assigned to a slot
// within `maps[]` table, so that the led will be turned on or off as indicated by the
// host. Otherwise, it behaves the same as CAPSLOCK. If more functionality is wanted in
// addition to turning on or off the lamp you can derive from `lamp_t` and override
// lamp_on/off() method. Beware that `lamp_t` is required to be the top-level keymap that
// is directly assigned to a slot within `maps[]` table.
template <class K>
class lamp_t: public map_lamp_t {
public:
    constexpr lamp_t(lamp_id_t lamp, K&& key, hsv_t hsv =RGB_INDICATOR_COLOR)
    : map_lamp_t(lamp, hsv), m_key(std::forward<K>(key)) {}

private:
    K m_key;

    // It behaves the same as `m_key` does by default, though can be overridden.
    virtual void on_press(pmap_t* slot) { m_key.press(slot); }
    virtual void on_release(pmap_t* slot) { m_key.release(slot); }
};

template <class K>
lamp_t(lamp_id_t, K&&, hsv_t) -> lamp_t<obj_or_ref_t<K>>;

template <class K>
lamp_t(lamp_id_t, K&&) -> lamp_t<obj_or_ref_t<K>>;

}  // namespace key
