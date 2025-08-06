-- Core key mapping engine for the "keymap" module.

-- Only the `base` and `package` modules are preloaded and registered in the global
-- environment. We need to explicitly "require" the `fw` module.
local fw = require "fw"

-- Note: Any runtime error in this script will cause a crash during the firmware boot.
-- For example, the `package` module does not define non_existent_function() and thus
-- `package.non_existent_function("nop")` will cause a crash!



-------- Configuration
-- TAPPING_TERM_MS defines the maximum interval between press and release for a key input
-- to be recognized as a tap. Used as the default tapping_term_ms value in TapHold and
-- TapSeq classes.
TAPPING_TERM_MS = 200

-------- Global variables
g_current_slot_index = 0
g_keymaps = {}

-------- Lightweight class implementation with multiple inheritance
local function class(...)
    local parents = {...}
    -- A class is a table used as the metatable for its instances.
    -- Multiple inheritance is implemented by customizing the __index metamethod to
    -- search parent classes.
    local cls = setmetatable({}, {  -- metatable for the class itself, not its instances.
        -- Setup inheritance through __index.
        __index = function(_, key)
            for _, parent in ipairs(parents) do
                local val = parent[key]
                if val then return val end
            end
            -- Return nothing if not found.
        end,

        -- Make the class callable to act as a constructor.
        __call = function(self, ...)
            -- Create a new instance (`obj`) and set its metatable to the class (`self`).
            local obj = setmetatable({}, self)
            -- If the class (`self`) defines its own init(), call it from the new object.
            -- Note: Child classes must explicitly call parent init() if needed.
            if rawget(self, "init") then
                obj:init(...)
            end
            return obj
        end
    })

    -- Allow instances to access methods defined in the class table.
    cls.__index = cls

    -- Note: Custom metamethods can be defined per class.
    -- Example: A = class(); function A:__eq(other) ... end
    return cls
end

-------- Base
-- Base class for all keymaps.
Base = class()

function Base:init()
    -- Initialize the new instance (`self`).
    self.m_press_count = 0
end

-- The base class provides virtual methods for processing press and release events, but
-- does not trigger keys actually.
function Base:on_press() end
function Base:on_release() end

function Base:_press()
    self.m_press_count = self.m_press_count + 1
    if self.m_press_count == 1 then
        self:on_press()
    end
end

function Base:_release()
    self.m_press_count = self.m_press_count - 1
    if self.m_press_count == 0 then
        self:on_release()
    end
end

function Base:is_pressed()
    return self.m_press_count > 0
end

Pseudo = Base  -- Base can be used standalone.

-------- Lit
Lit = class(Base)

-- Construct a keymap instance that triggers presses/releases for a named key. See
-- keycode_to_name[] for available key names.
-- Note: Each keymap instance is independent and tracks its own press/release state.
-- Although calling Lit() multiple times with the same key name is allowed, it is
-- generally discouraged. If you want multiple key slots to behave identically (e.g.
-- left and right FN), create a single instance and assign it to both slots.
function Lit:init(keyname)
    Base.init(self)
    local keycode = fw.keycode(keyname)
    if not keycode then
        error('invalid keyname "'..keyname..'"')
    end
    self.m_keycode = keycode
end

function Lit:on_press()
    fw.send_key(self.m_keycode, true)
end

function Lit:on_release()
    fw.send_key(self.m_keycode, false)
end

-------- Function
-- Function(func1 [, func2]) invokes func1() on press and optionally func2() on release.
Function = class(Base)

function Function:init(func1, func2)
    Base.init(self)
    self.m_func1 = func1
    self.m_func2 = func2
end

function Function:on_press()
    self.m_func1()
end

function Function:on_release()
    if self.m_func2 then
        self.m_func2()
    end
end

-------- OneShot
-- Force the given keymap to trigger as a quick tap, ignoring holds.
OneShot = class(Base)

function OneShot:init(map)
    Base.init(self)
    self.m_map = map
end

function OneShot:on_press()
    self.m_map:_press()
    self.m_map:_release()
end

-------- Proxy
-- If a keymap class inherits from Proxy, the keymap driver calls on_proxy_press/
-- release() instead of on_press/release(). These proxy methods can layer additional
-- logic on top of the base methods.
Proxy = class(Base)

function Proxy:init()
    Base.init(self)
end

function Proxy:on_proxy_press() end
function Proxy:on_proxy_release() end

function Proxy:_press()
    self.m_press_count = self.m_press_count + 1
    if self.m_press_count == 1 then
        self:on_proxy_press()
    end
end

function Proxy:_release()
    self.m_press_count = self.m_press_count - 1
    if self.m_press_count == 0 then
        self:on_proxy_release()
    end
end

-------- Modifier
-- On a press/release event, it calls on_modified_press/release() if the specified
-- modifier is currently pressed, or on_press/release() otherwise.
Modifier = class(Proxy)

function Modifier:init(map_modifier)
    Proxy.init(self)
    self.m_map_modifier = map_modifier
    self.m_is_modified = false
end

function Modifier:on_modified_press() end
function Modifier:on_modified_release() end

function Modifier:on_proxy_press()
    assert( self.m_is_modified == false )
    if self.m_map_modifier:is_pressed() then
        self.m_is_modified = true
        self:on_modified_press()
    else
        self:on_press()
    end
end

function Modifier:on_proxy_release()
    if self.m_is_modified then
        self:on_modified_release()
        self.m_is_modified = false
    else
        self:on_release()
    end
end

-------- ModIf
-- ModIf(map_modifier, map_modified, map_original) acts as map_modified if map_modifier
-- is currently pressed, or map_original otherwise.
ModIf = class(Modifier)

-- Flavors
KEEP_MODIFIER = 0   -- (default)
-- The modifier remains pressed when map_modified is triggered.

UNDO_MODIFIER = 1
-- The modifier is released and then pressed again when map_modified is triggered.

function ModIf:init(map_modifier, map_modified, map_original, flavor)
    Modifier.init(self, map_modifier)
    self.m_map_modified = map_modified
    self.m_map_original = map_original
    self.m_flavor = flavor or KEEP_MODIFIER
end

function ModIf:on_press()
    self.m_map_original:_press()
end

function ModIf:on_release()
    self.m_map_original:_release()
end

function ModIf:on_modified_press()
    if self.m_flavor == UNDO_MODIFIER then
        self.m_map_modifier:_release()
    end
    self.m_map_modified:_press()
end

function ModIf:on_modified_release()
    self.m_map_modified:_release()
    if self.m_flavor == UNDO_MODIFIER then
        -- Since Base.m_press_count is a signed integer, calling _press() here won't
        -- re-press the modifier if it's already been released externally.
        self.m_map_modifier:_press()
    end
end

-------- Defer
-- Defer Mode:
-- When active, defer mode causes all key events—except those targeting the "deferrer"—
-- to be deferred rather than processed immediately. If the deferrer keymap is formed
-- from nested keymaps, they are linked together in a chain starting from the outermost
-- keymap. The outermost keymap (i.e. Defer.owner) is referred to as the "deferrer",
-- while the complete chain—including the outermost and all nested keymaps—is
-- collectively referred to as the "deferrer chain" or simply the "deferrers". Note that
-- they all sit in the same slot.
--
-- Deferred events remain queued and do *not* trigger their usual on_press()/release()
-- methods. Instead, they are redirected to the deferrers, invoking their
-- on_other_press()/release() methods. This effectively routes all key events to the
-- deferrers during defer mode.
--
-- When a deferrer exits defer mode by calling stop_defer(), it's removed from the
-- deferrer chain and will stop receiving deferred events. Once all deferrers exit,
-- queued events are processed normally via their standard on_press()/release() methods.
--
-- Note: When multiple deferrers exist, on_other_press/release() methods are called
-- from outermost to innermost—unless an outer deferrer stops deferring the deferred
-- event by returning a non-nil value from its on_other_press()/release().

Defer = class()

-- Class variables shared with all instances
Defer.owner = false      -- Defer owner is the head of the deferrer chain.
Defer.slot_index = 0     -- Key slot index (common to all deferrers)

function Defer:init()
    self.m_next = false  -- Link to the next deferrer
end

function Defer:on_other_press() end
function Defer:on_other_release() end

function Defer:start_defer()  -- Start defer mode.
    fw.log("Map: start defer\n")
    self.m_next = false

    if not Defer.owner then
        Defer.owner = self
        Defer.slot_index = g_current_slot_index
        fw.defer_start()
        return
    end

    assert( Defer.slot_index == g_current_slot_index, "defer not stopped correctly?" )
    local deferrer = Defer.owner
    while deferrer.m_next do
        deferrer = deferrer.m_next
    end
    deferrer.m_next = self
end

-- Any deferrer in the chain can stop defer mode, in any sequence.
function Defer:stop_defer()  -- Stop defer mode.
    fw.log("Map: stop defer\n")
    local deferrer = Defer.owner

    if deferrer == self then
        Defer.owner = self.m_next
        -- Here self.m_next is left intact so that Defer:_other_event() can continue to
        -- traverse the downstream deferrers, even if the current deferrer called
        -- stop_defer() from its on_other_press/release().
        if not Defer.owner then
            Defer.slot_index = 0
            fw.defer_stop()
        end
        return
    end

    while deferrer do
        if deferrer.m_next == self then
            deferrer.m_next = self.m_next
            return
        end
        deferrer = deferrer.m_next
    end

    assert( nil, "deferrer not found" )
end

function Defer:_other_event(is_press)
    local deferrer = Defer.owner
    assert( deferrer )

    -- Traverse the chain to call on_other_press/release() from outer to inner.
    while deferrer do
        local result
        if is_press then
            result = deferrer:on_other_press()
        else
            result = deferrer:on_other_release()
        end

        -- If a deferrer chooses to handle the event immediately, stop traversal and
        -- return its result (true).
        if result then return result end
        deferrer = deferrer.m_next
    end
end

-------- Timer
Timer = class()

function Timer:init(timeout_ms)
    assert( timeout_ms % 1 == 0, "timeout_ms not an integer" )
    -- m_ctimer holds a userdata instance of the internal C++ class `_timer_t`.
    self.m_ctimer = fw.timer_create(timeout_ms)
    -- Closure used as the timer callback; captures `self` for invoking on_timeout().
    self.m_callback = function() self:on_timeout() end

    -- Note: No __gc() needed for this class; the callback is only referenced between
    -- fw.timer_start() and fw.timer_stop(), so the Timer instance won't be collected
    -- while the timer is active.
end

function Timer:on_timeout() end  -- Will be invoked on timeout.

function Timer:start_timer()
    fw.timer_start(self.m_ctimer, self.m_callback)
end

function Timer:stop_timer()
    fw.timer_stop(self.m_ctimer)
end

function Timer:timer_is_running()
    return fw.timer_is_running(self.m_ctimer)
end

-------- TapHold
-- TapHold(key1, key2 [, flavor] [, tapping_term_ms]) behaves as either key1 or key2,
-- depending on whether the key is tapped or held:
--  - If tapped (released before tapping_term_ms), key1 is sent.
--  - If held longer than tapping_term_ms, key2 is sent.
-- If any other key is pressed or released during tapping_term_ms, the decision depends
-- on the optional "interrupt" flavor. Multiple flavors can be combined using bitwise OR.

-- Interrupt flavors
TapOnPress      = 0x1
-- If any other key is pressed during tapping_term_ms, the tap behavior is triggered
-- (key1 is tapped).

HoldOnPress     = 0x2
-- If any other key is pressed during tapping_term_ms, the hold behavior is triggered
-- (key2 is held). This prioritizes over TapOnPress.

TapOnRelease    = 0x4
-- If any other key is released during tapping_term_ms, the tap behavior is triggered
-- (key1 is tapped).

HoldOnRelease   = 0x8
-- If any other key is pressed during tapping_term_ms, the hold behavior is triggered
-- (key2 is held). This prioritizes over TapOnRelease.

QuickRelease    = 0x10
-- If any other key was pressed before the TapHold key and then released during
-- tapping_term_ms, it is released immediately and does not affect the tap/hold decision.
-- This flavor can be combined with TapOnRelease and HoldOnRelease to limit their
-- effects to the (other) keys that are both pressed and released during tapping_term_ms.

HoldIsTap       = 0x20
-- When the hold behavior is triggered, key2 is tapped as OneShot instead of being held.

TapHold = class(Base, Defer, Timer)

function TapHold:init(map_tap, map_hold, flavor, tapping_term_ms)
    Base.init(self)
    Defer.init(self)
    Timer.init(self, tapping_term_ms or TAPPING_TERM_MS)

    self.m_map_tap = map_tap
    if self.m_flavor == HoldIsTap then
        self.m_map_hold = OneShot(map_hold)
    else
        self.m_map_hold = map_hold
    end
    self.m_map_chosen = false
    self.m_flavor = flavor or 0
    self.m_my_slot = 0  -- used for debugging purposes
end

function TapHold:help_decide(map_to_choose)
    -- stop_timer(), _press(), and stop_defer() are order-independent and can be called
    -- in any sequence.
    self:stop_timer()
    map_to_choose:_press()
    self.m_map_chosen = map_to_choose
    self:stop_defer()
end

function TapHold:decide_tap()
    self:help_decide(self.m_map_tap)
end

function TapHold:decide_hold()
    self:help_decide(self.m_map_hold)
end

function TapHold:on_press()
    self.m_my_slot = g_current_slot_index
    fw.log("TapHold [%d] on_press()\n", self.m_my_slot)
    assert( self.m_map_chosen == false )
    self:start_timer()
    self:start_defer()
end

function TapHold:on_release()
    fw.log("TapHold [%d] on_release()\n", self.m_my_slot)
    if not self.m_map_chosen then
        -- Note: During defer mode, on_release()—rather than on_other_release()—is called
        -- on the deferrer (Defer.owner). In this case, the deferrer's on_release() may
        -- be invoked *before* any deferred events are processed, potentially disrupting
        -- the original event order—e.g. the deferrer's release occurring before other
        -- presses.
        --
        -- This is acceptable because on release, we trigger both _press() and _release()
        -- together for m_map_tap (the tapping key), not for m_map_hold (the holding
        -- key), and the tapping key’s release timing isn't critical.
        --
        -- However, if the tapping key were sensitive to release timing (e.g. a
        -- modifier), we would need to modify the keymap driver to notify the deferrer
        -- twice upon its release: once to trigger the tapping key's press (via e.g.
        -- bool on_early_release()), and again to trigger its normal release (via
        -- on_release()).
        fw.log("TapHold [%d] decide tap on release\n", self.m_my_slot)
        self:decide_tap()
    end

    self.m_map_chosen:_release()
    self.m_map_chosen = false
end

function TapHold:on_timeout()
    fw.log("TapHold [%d] decide hold on timeout\n", self.m_my_slot)
    self:decide_hold()
end

function TapHold:on_other_press()
    if self.m_flavor & HoldOnPress ~= 0 then
        fw.log("TapHold [%d] decide hold on other press\n", self.m_my_slot)
        self:decide_hold()

    elseif self.m_flavor & TapOnPress ~= 0 then
        fw.log("TapHold [%d] decide tap on other press\n", self.m_my_slot)
        self:decide_tap()
    end
end

function TapHold:on_other_release()
    if self.m_flavor & QuickRelease ~= 0 then
        if not fw.defer_is_pending(g_current_slot_index, true) then
            -- Note: Returning true here releases the other key immediately, while
            -- delaying the deferrer's tap/hold decision. This causes the press for
            -- m_map_tap or m_map_hold to occur *after* the other key's release,
            -- disrupting the original event order. While this is generally acceptable,
            -- it may cause unintended behavior—for example, if the other key is a
            -- modifier, it could be released prematurely.
            return true
        end
    end

    if self.m_flavor & HoldOnRelease ~= 0 then
        fw.log("TapHold [%d] decide hold on other release\n", self.m_my_slot)
        self:decide_hold()

    elseif self.m_flavor & TapOnRelease ~= 0 then
        fw.log("TapHold [%d] decide tap on other release\n", self.m_my_slot)
        self:decide_tap()
    end
end

-------- TapDance
-- Similar to QMK's ACTION_TAP_DANCE_FN_ADVANCED(), only function names differ.
--  • on_press() is called each time the TapDance key is pressed.
--  • on_finish() is invoked when the tap dance finishes:
--      - either after tapping_term_ms has elapsed since the last press,
--      - or when any other key is pressed within tapping_term_ms.
--    Then on_release() is called immediately after.
--  • Alternatively, calling finish() manually (e.g. from on_press()) finishes the tap
--    dance. In this case, on_finish() is skipped and on_release() will be called later
--    when the TapDance key is physically released.
--  • Thus, on_release() is always called - either after on_finish(), or following a
--    manual finish() plus key release.
--  • Typical call sequence:
--    on_press(), on_press(), ..., [on_finish()], on_release().
--  • Use self.m_step in on_press/finish/release() to get the current tap count.
TapDance = class(Proxy, Defer, Timer)

function TapDance:init(tapping_term_ms)
    Proxy.init(self)
    Defer.init(self)
    -- (TAPPING_TERM_MS - 2) is used here instead of TAPPING_TERM_MS. Although
    -- TAPPING_TERM_MS works fine when nesting TapHold inside TapSeq - since TapSeq's
    -- timer starts earlier and expires first - slightly reducing TapSeq's timeout helps
    -- prevent spurious timer expirations.
    Timer.init(self, tapping_term_ms or (TAPPING_TERM_MS - 2))

    -- m_step is incremented just before each on_press() call, and reset to 0 after
    -- on_release().
    self.m_step = 0
    -- Although m_step = 0 could signify the dance has finished, we use a separate flag
    -- so that on_release() can still access m_step during its execution.
    self.m_is_finished = true
    self.m_my_slot = 0  -- used for debugging purposes
end

-- Todo: Would it be better to add a parameter to on_finish() to indicate whether
-- on_finish() was called from on_timeout() or from on_other_press() (i.e. interrupted by
-- other key)?
function TapDance:on_finish() end

function TapDance:finish()
    fw.log("TapDance [%d] finish()\n", self.m_my_slot)
    -- Will skip calling on_finish().
    self:stop_timer()
    self:stop_defer()
    self.m_is_finished = true
end

function TapDance:on_proxy_press()
    self.m_step = self.m_step + 1
    self.m_my_slot = g_current_slot_index
    fw.log("TapDance [%d] on_proxy_press() m_step=%d\n", self.m_my_slot, self.m_step)
    if self.m_step == 1 then
        assert( self.m_is_finished )
        self.m_is_finished = false  -- Start the dance.
        self:start_defer()
    end

    self:start_timer()  -- (Re)start the timer.
    self:on_press()
end

function TapDance:on_proxy_release()
    fw.log("TapDance [%d] on_proxy_release() m_step=%d\n", self.m_my_slot, self.m_step)
    if self.m_is_finished then
        self:on_release()
        self.m_step = 0
    end
end

function TapDance:on_timeout()
    fw.log("TapDance [%d] on_other_press/timeout()\n", self.m_my_slot)
    self:stop_defer()
    self.m_is_finished = true  -- Finish the dance.
    self:on_finish()
    if not self:is_pressed() then
        self:on_release()  -- Trigger a late release notification.
        self.m_step = 0
    end
end

function TapDance:on_other_press()
    self:stop_timer()
    self:on_timeout()
end

-------- TapSeq
-- TapSeq(key1, key2, ..., [tapping_term_ms]) triggers sequential tap actions based on
-- the number of presses:
--   - map_tap[1] is sent on the first tap,
--   - map_tap[2] on the second tap,
--   - and so on.
-- If the TapSeq key is held at step n (i.e. pressed and held beyond tapping_term_ms),
-- then map_tap[n] is pressed and held instead of tapped.
TapSeq = class(TapDance)

function TapSeq:init(...)
    local args = {...}
    local tapping_term_ms = nil
    if type(args[#args]) == "number" then
        tapping_term_ms = args[#args]
        args[#args] = nil
    end

    TapDance.init(self, tapping_term_ms)
    self.m_map_tap = args
end

function TapSeq:on_press()
    if self.m_step > 1 then
        self.m_map_tap[self.m_step - 1]:_release()
    end

    self.m_map_tap[self.m_step]:_press()

    if self.m_step == #self.m_map_tap then
        self:finish()
    end
end

function TapSeq:on_release()
    self.m_map_tap[self.m_step]:_release()
end



-- Manually assign a non-nil value to package.loaded["keymap"], so require("keymap")
-- returns this value. Necessary because the firmware uses a simplified require() that
-- does not automatically fill package.loaded[].
package.loaded[...] = true  -- Vararg (...) here receives a single argument, "keymap".

-- Core keymap driver (engine) responsible for processing key events and dispatching
-- them to user-defined mappings. This function is returned and passed to the next
-- script in the compilation chain.
return function(slot_index, is_press)
    g_current_slot_index = slot_index
    local press_or_release = is_press and "press" or "release"

    local deferrer = Defer.owner
    if deferrer then
        -- In defer mode, if the event targets the deferrer, execute it immediately.
        if slot_index == Defer.slot_index then
            fw.log("Map: [%d] handle deferrer %s\n", slot_index, press_or_release)

        -- In defer mode, if the event targets a slot other than the deferrer's, notify
        -- the deferrer first.
        else
            fw.log("Map: [%d] handle other %s @[%d]\n",
                Defer.slot_index, press_or_release, slot_index)
            if not deferrer:_other_event(is_press) then
                return
            end

            -- If the deferrer has opted not to defer this event by returning a non-nil,
            -- execute it immediately.
            fw.log("Map: [%d] execute immediate %s\n", slot_index, press_or_release)
        end

        -- Note: Those two cases of executing events immediately during defer mode can
        -- disrupt the original key event order. See notes in TapHold.

    -- Execute the event if in normal mode.
    else
        fw.log("Map: [%d] handle %s\n", slot_index, press_or_release)
    end

    -- rgb_thread::obj().signal_key_event(slot_index, is_press)

    if is_press then
        g_keymaps[slot_index]:_press()
    else
        g_keymaps[slot_index]:_release()
    end

    -- If we were in defer mode, remove the latest deferred event from the event queue,
    -- since it has already executed - either on the deferrer or on another slot.
    -- Note: This deferred event (just peeked) should have .slot_index equal to the
    -- current slot_index. See the while-loop in main_thread.
    if deferrer then
        fw.defer_remove_last()
    end
end
