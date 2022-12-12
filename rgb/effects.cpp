#include <cmath>                // for std::sqrt() for floats

#define ENABLE_DEBUG    (0)
#include "debug.h"

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



template <size_t N>
typename touched_leds_t<N>::iterator touched_leds_t<N>::create(uint8_t led_id)
{
    iterator it = m_array.begin();
    for ( ; it != m_array.end() ; ++it )
        if ( it->state == DONE ) {
            if ( it == m_actual_end )
                ++m_actual_end;

            // Initialize the members.
            it->when_released_ms = 0;
            it->led_id = led_id;
            it->state = PRESSED;
            break;
        }

    return it;
}

template <size_t N>
typename touched_leds_t<N>::iterator touched_leds_t<N>::find(
    uint8_t led_id, bool pressed_only)
{
    iterator it = begin();
    for ( ; it != end() ; ++it )
        // If pressed_only is true only the (first) element with PRESSED state is found
        // as the tie breaker.
        if ( it->led_id == led_id && (!pressed_only || it->state == PRESSED) )
            break;

    return it;
}

template <size_t N>
void touched_leds_t<N>::collect_garbage(bool handle_updating)
{
    // If handle_updating is true all non-PRESSED states are automatically moved down;
    // RELEASED -> DONE and UPDATING -> RELEASED. So, to not erase elements their states
    // need to keep updating as UPDATING, which is the case of ripple::update().
    if ( handle_updating )
        for ( iterator it = begin() ; it != end() ; ++it ) {
            if ( it->state == RELEASED )
                it->state = DONE;
            else if ( it->state == UPDATING )
                it->state = RELEASED;
        }

    while ( m_actual_end != begin() && (m_actual_end - 1)->state == DONE )
        --m_actual_end;
}



finger_trace::finger_trace(hsv_t hsv, uint32_t restoring_ms)
: m_hsv(hsv), m_restoring_ms(restoring_ms)
{}

hsv_t finger_trace::initial_update(uint8_t)
{
    return hsv_t{ m_hsv.h, m_hsv.s, CIE1931_CURVE[m_hsv.v] };
}

ohsv_t finger_trace::process_key_event(uint8_t led_id, uint32_t time, bool pressed)
{
    // Note that m_touched_leds keeps only one element per led_id, changing the state
    // as PRESSED or RELEASED.
    auto it = m_touched_leds.find(led_id, false);

    if ( pressed ) {
        if ( it == m_touched_leds.end() ) {
            DEBUG("effect: create touched_led (%d)\n", led_id);
            m_touched_leds.create(led_id);
        } else
            it->state = PRESSED;

        return hsv_t{ m_hsv.h, m_hsv.s, 0 };  // Turn it off.
    }

    if ( it != m_touched_leds.end() ) {
        it->when_released_ms = time;
        it->state = RELEASED;
        enable_update_next(m_timer);
        return {};  // Keep it turned off.
    }

    // If out of tracers turn it on immediately on release.
    return initial_update(led_id);
}

void finger_trace::update_done()
{
    m_touched_leds.collect_garbage(false);
    if ( !m_touched_leds.empty() )
        enable_update_next(m_timer);
    else
        DEBUG("effect: touched_leds all removed\n");
}

ohsv_t finger_trace::update(uint8_t led_id, uint32_t time)
{
    assert( !m_touched_leds.empty() );
    auto it = m_touched_leds.find(led_id, false);

    if ( it == m_touched_leds.end() || it->state != RELEASED )
        return {};  // Do not update irrelevant leds.

    uint32_t dt = time - it->when_released_ms;
    const uint8_t v = CIE1931_CURVE[
        dt < m_restoring_ms
        ? m_hsv.v * dt / m_restoring_ms
        : (it->state = DONE, m_hsv.v) ];

    return hsv_t{ m_hsv.h, m_hsv.s, v };
}



ripple::ripple(hsv_t hsv, uint32_t period_ms, uint32_t wavelength)
: m_hsv(hsv), m_period_ms(period_ms), m_wavelength(wavelength)
{}

hsv_t ripple::initial_update(uint8_t)
{
    return hsv_t{ m_hsv.h, m_hsv.s, CIE1931_CURVE[m_hsv.v] };
}

ohsv_t ripple::process_key_event(uint8_t led_id, uint32_t time, bool pressed)
{
    // Note that there can be multiple elements (waves) for the same led_id, created once
    // per every press. But, there can be only element that has PRESSED state, all the
    // others will have RELEASED state.
    if ( pressed ) {
        m_touched_leds.create(led_id);
        DEBUG("effect: create touched_led (%d)\n", led_id);
        return hsv_t{ m_hsv.h, m_hsv.s, 0 };  // Turn it off.
    }

    auto it = m_touched_leds.find(led_id, true);
    if ( it != m_touched_leds.end() ) {
        it->when_released_ms = time;
        it->state = RELEASED;
        enable_update_next(m_timer);
        return {};  // Keep it turned off.
    }

    // If out of waves (resources) turn it on immediately on release.
    return initial_update(led_id);
}

void ripple::update_done()
{
    m_touched_leds.collect_garbage(true);
    if ( !m_touched_leds.empty() )
        enable_update_next(m_timer);
    else
        DEBUG("effect: touched_leds all removed\n");
}

ohsv_t ripple::update(uint8_t led_id, uint32_t time)
{
    assert( !m_touched_leds.empty() );
    const auto [px, py] = led_conf.point[led_id];
    unsigned depth = 0;  // <= m_hsv.v

    for ( auto it = m_touched_leds.begin() ; it != m_touched_leds.end() ; ++it ) {
        if ( it->state == DONE )
            continue;  // Ignore it since erased.

        if ( it->state == PRESSED ) {
            if ( it->led_id == led_id )
                return {};  // Keep it turned off.
            else
                continue;  // This wave didn't start yet. Ignore it.
        }

        const auto [center_x, center_y] = led_conf.point[it->led_id];
        uint32_t dist = std::sqrt(0.0f +  // to convert into floating-point.
            (px - center_x)*(px - center_x) + (py - center_y)*(py - center_y) );
        uint32_t t_reach = dist * m_period_ms / m_wavelength;
        uint32_t dt = time - it->when_released_ms;

        if ( dt < t_reach + m_period_ms ) {
            it->state = UPDATING;
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
    return hsv_t{ m_hsv.h, m_hsv.s, CIE1931_CURVE[m_hsv.v - uint8_t(depth)] };
}
