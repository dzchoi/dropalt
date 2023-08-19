#include "lamp.hpp"
#include "lamp_id.hpp"          // for is_lamp_lit()



ohsv_t lamp_t::is_lit() const
{
    return is_lamp_lit(m_lamp_id) ? ohsv_t(m_hsv) : ohsv_t();
}

void lamp_t::execute_lamp()
{
    is_lamp_lit(m_lamp_id) ? when_lamp_on(m_slot) : when_lamp_off(m_slot);
}

lamp_iter lamp_iter::operator++(int)
{
    lamp_iter it = *this;
    m_ptr = m_ptr->m_next;
    return it;
}
