#pragma once

#include "assert.h"             // for assert()

#include "keymap_thread.hpp"    // for key_events().start/stop_defer(), ...
#include "pmap.hpp"             // for index()



namespace key {

class defer_t {
protected: // Methods to be used by child classes
    constexpr defer_t() =default;

    // Not movable during run-time. See the comment in timer.hpp for using assert() in
    // constexpr functions.
    constexpr defer_t(defer_t&&) { assert( m_slot == nullptr ); }

    // Start Defer mode. All the following key events will be deferred until we stop the
    // Defer mode.
    void start_defer(pmap_t* slot) {
        m_slot = slot;
        keymap_thread::key_events().start_defer(this);
    }

    // Stop Defer mode. Any deferred events during the Defer mode will be played back as
    // next events.
    void stop_defer() {
        m_slot = nullptr;
        keymap_thread::key_events().stop_defer();
    }

    static bool is_deferred(pmap_t* slot, bool is_press) {
        return keymap_thread::key_events().is_deferred(slot->index(), is_press);
    }

    pmap_t* m_slot = nullptr;  // Which slot is deferring?

private: // Methods to be called by keymap_thread
    friend class ::keymap_thread;

    // [Defer mode]
    // During Defer mode all key events except those of the "deferrer" who has started
    // the Defer mode (key::events_t::m_deferrer) are deferred instead of being
    // processed normally; they do not trigger their own on_press/release() methods as
    // they would in normal mode, but instead trigger on_other_press/release() of the
    // deferrer. They will be processed later when the deferrer stops the Defer mode
    // (if on_other_press/release() returns false), then invoking their own on_press/
    // release(). If on_other_press/release() returns true deferred events will be
    // processed immediately after, rather than being deferred.
    //
    // The deferrer's own key events that occur during Defer mode are also processed
    // immediately, invoking the deferrer's on_press/release() instead of on_other_press/
    // release(). So note that during Defer mode all key events will be sent to the
    // deferrer exclusively.
    //
    // Be aware that those two cases of executing events immediately during Defer mode
    // can break the order of key event occurrences. See the comments in tap_hold.hpp.
    virtual bool on_other_press(pmap_t*) { return false; }

    virtual bool on_other_release(pmap_t*) { return false; }
};

}  // namespace key
