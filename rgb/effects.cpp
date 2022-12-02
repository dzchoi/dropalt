#include <cmath>                // for std::sqrt()

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include <algorithm>            // for std::find_if()
#include "color.hpp"            // for CIE1931_CURVE[]
#include "effects.hpp"
#include "led_conf.hpp"         // for led_conf.point[]



hsv_t glimmer::initial_update(uint8_t)
{
    return hsv_t{ m_hsv.h, m_hsv.s, 0 };
}

void glimmer::initial_update_done()
{
    // Start to update from the beginning.
    enable_update_next(m_timer);
}

ohsv_t glimmer::update(uint8_t led_id, uint32_t time)
{
    if ( led_id == 0 ) {
        // Remember the start time as offset_ms to be able to start updating from zero.
        static uint32_t offset_ms = time;

        // Fit dt to [0, m_period_ms).
        uint32_t dt = time - offset_ms;
        while ( dt >= m_period_ms ) {
            dt -= m_period_ms;
            offset_ms += m_period_ms;
        }

        // Linear curve with the peak (=m_hsv.v) at m_period_ms/2.
        if ( dt < m_period_ms / 2 )
            m_v = CIE1931_CURVE[(2u*m_hsv.v) * dt / m_period_ms];
        else
            m_v = CIE1931_CURVE[(2u*m_hsv.v - 1) - (2u*m_hsv.v) * dt / m_period_ms];
    }

    return hsv_t{ m_hsv.h, m_hsv.s, m_v };
};

void glimmer::update_done()
{
    enable_update_next(m_timer);
}



finger_trace::finger_trace(hsv_t hsv, uint32_t restoring_ms)
: m_hsv(hsv), m_restoring_ms(restoring_ms)
{
    m_touched_leds.reserve(32);
}

hsv_t finger_trace::initial_update(uint8_t)
{
    return hsv_t{ m_hsv.h, m_hsv.s, CIE1931_CURVE[m_hsv.v] };
}

ohsv_t finger_trace::process_key_event(uint8_t led_id, uint32_t time, bool pressed)
{
    auto it = std::find_if(m_touched_leds.begin(), m_touched_leds.end(),
        [=](const touched_t& led) { return led.led_id == led_id; } );

    if ( pressed ) {
        if ( it == m_touched_leds.end() ) {
            DEBUG("effect: create touched_led (%d), count=%d\n",
                led_id, m_touched_leds.size() + 1);
            m_touched_leds.push_back(touched_t{ 0, led_id, touched_t::PRESSED });
        } else
            it->state = touched_t::PRESSED;

        return hsv_t{ m_hsv.h, m_hsv.s, 0 };  // Turn it off.
    }

    assert( it != m_touched_leds.end() );
    it->when_released_ms = time;
    it->state = touched_t::RELEASED;
    enable_update_next(m_timer);
    return {};  // Keep it turned off.
}

void finger_trace::update_done()
{
    assert( !m_touched_leds.empty() );
    auto it = m_touched_leds.begin();
    while ( it != m_touched_leds.end() ) {
        if ( it->state == touched_t::FINISHED ) {
            DEBUG("effect: remove touched_led (%d), count=%d\n",
                it->led_id, m_touched_leds.size() - 1);
            it = m_touched_leds.erase(it);
        } else
            ++it;
    }

    if ( !m_touched_leds.empty() )
        enable_update_next(m_timer);
}

ohsv_t finger_trace::update(uint8_t led_id, uint32_t time)
{
    assert( !m_touched_leds.empty() );
    auto it = std::find_if(m_touched_leds.begin(), m_touched_leds.end(),
        [=](const touched_t& led) { return led.led_id == led_id; } );

    if ( it == m_touched_leds.end() || it->state != touched_t::RELEASED )
        return {};  // Do not update irrelevant leds.

    uint32_t dt = time - it->when_released_ms;
    const uint8_t v = CIE1931_CURVE[
        dt < m_restoring_ms
        ? m_hsv.v * dt / m_restoring_ms
        : (it->state = touched_t::FINISHED, m_hsv.v) ];

    return hsv_t{ m_hsv.h, m_hsv.s, v };
}



ripple::ripple(hsv_t hsv, uint32_t period_ms, uint32_t wavelength)
: m_hsv(hsv), m_period_ms(period_ms), m_wavelength(wavelength)
{
    m_touched_leds.reserve(16);
}

hsv_t ripple::initial_update(uint8_t)
{
    return hsv_t{ m_hsv.h, m_hsv.s, CIE1931_CURVE[m_hsv.v] };
}

ohsv_t ripple::process_key_event(uint8_t led_id, uint32_t time, bool pressed)
{
    if ( pressed ) {
        DEBUG("effect: create touched_led (%d), count=%d\n",
            led_id, m_touched_leds.size() + 1);
        m_touched_leds.insert(m_touched_leds.begin(),
            touched_t{ 0, led_id, touched_t::PRESSED });

        return hsv_t{ m_hsv.h, m_hsv.s, 0 };  // Turn it off.
    }

    auto it = std::find_if(m_touched_leds.begin(), m_touched_leds.end(),
        [=](const touched_t& led) { return led.led_id == led_id; } );
    assert( it != m_touched_leds.end() );

    it->when_released_ms = time;
    it->state = touched_t::RELEASED;
    enable_update_next(m_timer);
    return {};  // Keep it turned off.
}

// Same as finger_trace::update_done().
void ripple::update_done()
{
    assert( !m_touched_leds.empty() );
    auto it = m_touched_leds.begin();
    while ( it != m_touched_leds.end() ) {
        if ( it->state == touched_t::FINISHED ) {
            DEBUG("effect: remove touched_led (%d), count=%d\n",
                it->led_id, m_touched_leds.size() - 1);
            it = m_touched_leds.erase(it);
        } else
            ++it;
    }

    if ( !m_touched_leds.empty() )
        enable_update_next(m_timer);
}

ohsv_t ripple::update(uint8_t led_id, uint32_t time)
{
    assert( !m_touched_leds.empty() );
    const auto [px, py] = led_conf.point[led_id];
    unsigned depth = 0;  // <= m_hsv.v

    for ( auto it = m_touched_leds.begin() ; it != m_touched_leds.end() ; ++it ) {
        if ( it->state == touched_t::PRESSED ) {
            if ( it->led_id == led_id )
                return {};  // Keep it turned off.
            else
                continue;  // This wave didn't start yet. Ignore it.
        }

        // Start with the FINISHED state. It will remain FINISHED if the wave does not
        // update any leds.
        if ( led_id == 0 )
            it->state = touched_t::FINISHED;

        const auto [center_x, center_y] = led_conf.point[it->led_id];
        uint32_t dist = std::sqrt(0.0f +  // to convert into floating-point.
            (px - center_x)*(px - center_x) + (py - center_y)*(py - center_y) );
        uint32_t t_reach = dist * m_period_ms / m_wavelength;
        uint32_t dt = time - it->when_released_ms;

        if ( dt < t_reach + m_period_ms ) {
            it->state = touched_t::RELEASED;
            if ( dt >= t_reach ) {
                dt -= t_reach;
                // Superposition of waves; the total depth is calculated to be the sum
                // of individual wave displacements (depths).
                if ( dt < m_period_ms / 2 )
                    depth += 2u * m_hsv.v * dt / m_period_ms;
                else
                    depth += 2u * m_hsv.v * (m_period_ms - dt) / m_period_ms;
            }
        }
    }

    if ( depth > m_hsv.v )
        depth = m_hsv.v;
    return hsv_t{ m_hsv.h, m_hsv.s, CIE1931_CURVE[m_hsv.v - uint8_t(depth)]};
}
