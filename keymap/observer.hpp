#pragma once

#include "whole.hpp"



namespace key {

class observer_t {
public:
    // Called when any other key gets pressed (before its on_press() is called.)
    // (This order of callings ensures that when a key enrolls as an observer in its
    // on_press() its press will not trigger its own on_other_press().)
    virtual void on_other_press(pmap_t*) {}

    // Called when any other key gets released (after its on_release() is called.)
    virtual void on_other_release(pmap_t*) {}

    void start_observe() { whole.enroll_observer(this); }

    void stop_observe() { whole.unenroll_observer(this); }

private:
    friend class whole_t;
    observer_t* next = nullptr;  // used by key::whole to manage observer list.
};

}  // namespace key
