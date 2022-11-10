#pragma once

#include "manager.hpp"          // for enroll/unenroll_observer()



namespace key {

class observer_t {
protected: // Methods to be used by child classes
    constexpr observer_t() =default;

    void start_observe() { manager.enroll_observer(this); }

    void stop_observe() { manager.unenroll_observer(this); }

private: // Methods to be called by key::manager
    friend class manager_t;

    // Called when any other key gets pressed (before its on_press() is called.)
    // (This order of callings ensures that when a key enrolls as an observer in its
    // on_press() its press will not trigger its own on_other_press().)
    virtual void on_other_press(pmap_t*) {}

    // Called when any other key gets released (after its on_release() is called.)
    virtual void on_other_release(pmap_t*) {}

    observer_t* next = nullptr;  // used by key::manager to manage observer list.
};

}  // namespace key
