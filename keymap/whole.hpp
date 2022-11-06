#pragma once



namespace key {

class observer_t;
class pmap_t;

// The key::whole represents and handles all keys. It executes on_press/release() of each
// individual key and also executes on_other_press/release() for observers.
class whole_t {
public:
    void on_press(pmap_t* ppmap);

    void on_release(pmap_t* ppmap);

    // Enrolled observers will get notified of any other key presses and releases.
    // Duplicate enrollment is discarded, returning false.
    bool enroll_observer(observer_t* observer);

    // Unenroll from observer list and it will no longer get notified. Return false if
    // it is not enrolled.
    bool unenroll_observer(observer_t* observer);

private:
    observer_t* m_observers = nullptr;
};

inline whole_t whole;

}  // namespace key
