#pragma once

#include "assert.h"             // for assert()

#include "manager.hpp"          // for enroll/unenroll_observer()



namespace key {

class observer_t {
protected: // Methods to be used by child classes
    constexpr observer_t() =default;

    // Not movable during run-time. See the comment in timer.hpp for using assert() in
    // constexpr functions.
    constexpr observer_t(observer_t&&) { assert( who == nullptr ); }

    void start_observe(pmap_t* slot) { who = slot; manager.enroll_observer(this); }

    void stop_observe() { manager.unenroll_observer(this); }

private: // Methods to be called by key::manager
    friend class manager_t;

    // Called when any other key gets pressed, before the key's on_press() is called.
    virtual void on_other_press(pmap_t*) {}

    // Called when any other key gets released, before the key's on_release() is called.
    virtual void on_other_release(pmap_t*) {}

    observer_t* next = nullptr;  // used by key::manager to manage observer list.
    pmap_t* who = nullptr;  // Who is observing?
};

}  // namespace key
