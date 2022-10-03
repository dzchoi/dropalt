#pragma once

#include "event.h"



// Extended event_t type
template <typename T, typename U =void>
struct event_ext_t: event_t {
    T arg0;
    U arg1;
};

template <typename T>
struct event_ext_t<T, void>: event_t {
    T arg;
};
