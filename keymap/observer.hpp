#pragma once

#include "keymap.hpp"



namespace key {

class observer_t;

class _any_t: public base_t {
public:
    void on_press(pbase_t* ppbase) {
        ++m_any_pressed;
        notify_of_press(ppbase);
    }

    void on_release(pbase_t* ppbase) {
        --m_any_pressed;
        notify_of_release(ppbase);
    }

    bool is_pressing() const { return m_any_pressed != 0; }

    // Enrolled observers will get notified of any other key presses and releases.
    // Duplicate enrollment is discarded, returning false.
    bool enroll(observer_t* observer);

    // Unenroll from observer list and it will no longer get notified. Returns false if
    // it is not enrolled.
    bool unenroll(observer_t* observer);

private:
    unsigned m_any_pressed = 0;
    observer_t* m_observers = nullptr;

    // Notify all observers.
    void notify_of_press(pbase_t* ppbase);

    void notify_of_release(pbase_t* ppbase);
};

inline _any_t ANY;



class observer_t {
public:
    // Called when any other key gets pressed (before its on_press() is called.)
    // (This order of callings ensures that when a key enrolls as an observer in its
    // on_press() its own press will not trigger its on_other_press().)
    virtual void on_other_press(pbase_t*) {}

    // Called when any other key gets released (after its on_release() is called.)
    virtual void on_other_release(pbase_t*) {}

    void start_observe() { ANY.enroll(this); }
    void stop_observe() { ANY.unenroll(this); }

private:
    friend class _any_t;
    observer_t* next = nullptr;  // used by ANY to manage observer list.
};

}  // namespace key
