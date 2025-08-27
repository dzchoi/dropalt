local fw = require "fw"



-------- Configuration
-- Rate for updating RGB leds to show effects. 17 ms corresponds to ~60 fps.
RGB_UPDATE_PERIOD_MS = 17

-------- LED constants from led_conf.h
HSV_HUE_STEPS     = 0x600
KEY_LED_COUNT     = 67  -- slot_index <= 67 corresponds to a key LED.
LED_BOTTOM_LEFT   = 68  -- slot_index >= 68 corresponds to an underglow LED.
LED_BOTTOM_RIGHT  = 82
LED_TOP_RIGHT     = 87
LED_TOP_LEFT      = 101
ALL_LED_COUNT     = 105

-------- Lamp ID
LAMP_NUMLOCK      = 1
LAMP_CAPSLOCK     = 2
LAMP_SCRLOCK      = 3
-- LAMP_COMPOSE      = 4
-- LAMP_KANA         = 5



-------- Effect
Effect = Class()

-- Set the default effect to Effect to disable LED activity.
-- Note: Do not override this default effect when ENABLE_RGB_LED is false.
Effect.c_active_effect = Effect
-- Tracks slot indices that should be excluded from Effect updates. Each entry can hold
-- one of three states:
--   - nil:   lamp is inactive
--   - false: lamp is inactive, but its activator key has been pressed
--   - true:  lamp is active
--
-- State transitions:
-- Activate/Inactivate may occur before or after Release, depending on the host OS.
-- Events: Press Activate Release  ...  Press Inactivate Release
-- State:  false*  true    true    ...   true    nil       nil
--
-- Events: Press Release Activate  ...  Press Release Inactivate
-- State:  false* false*   true    ...   true   true     nil
-- * Only these three cases execute on_press/release().
Effect.c_lamp_lit_slots = {}

function Effect:init() end

function Effect:on_press(slot_index) end
function Effect:on_release(slot_index) end
function Effect:on_lamp_active(slot_index) end
function Effect:on_lamp_inactive(slot_index) end

function Effect:_press(slot_index)
    if not Effect.c_lamp_lit_slots[slot_index] then
        Effect.c_lamp_lit_slots[slot_index] = false
        self:on_press(slot_index)
    end
end

function Effect:_release(slot_index)
    if Effect.c_lamp_lit_slots[slot_index] == false then
        self:on_release(slot_index)
    end
end

-------- "Fixed Color"
Fixed = Class(Effect)

-- Solid color LEDs; they will be turned on on USB resume.
function Fixed:init(...)
    Effect.init(self)

    self.m_hsv = {...}
    for slot_index = 1, KEY_LED_COUNT do
        fw.led_set_hsv(slot_index, ...)
        -- Or fw.led_set_hsv(slot_index, fw.unpack(self.m_hsv))
    end
    fw.led_refresh()
end

function Fixed:on_lamp_active(slot_index)
    -- If slot_index <= KEY_LED_COUNT, turn it off. Otherwise, turn it on.
    fw.led_set_hsv(slot_index, self.m_hsv[1], self.m_hsv[2],
        slot_index <= KEY_LED_COUNT and 0 or self.m_hsv[3])
    fw.led_refresh()
end

function Fixed:on_lamp_inactive(slot_index)
    fw.led_set_hsv(slot_index, self.m_hsv[1], self.m_hsv[2],
        slot_index <= KEY_LED_COUNT and self.m_hsv[3] or 0)
    fw.led_refresh()
end

-------- "Finger Tracer"
FingerTracer = Class(Fixed, Timer)

function FingerTracer:init(restore_ms, ...)
    Fixed.init(self, ...)
    Timer.init(self)

    self.m_restore_ms = restore_ms
    -- m_touched_leds[] stores (slot_index, release_ms) pairs for all slots that have
    -- been released and whose LEDs are currently fading in.
    self.m_touched_leds = {}
end

function FingerTracer:on_press(slot_index)
    fw.led_set_hsv(slot_index, self.m_hsv[1], self.m_hsv[2], 0)  -- Turn it off.
    fw.led_refresh()
    -- Do not update the LED while pressing.
    self.m_touched_leds[slot_index] = nil
end

function FingerTracer:on_release(slot_index)
    self.m_touched_leds[slot_index] =
        self:time_now() or self:start_timer(RGB_UPDATE_PERIOD_MS, true)
    -- Keep the LED turned off.
end

function FingerTracer:on_timeout(time_now_ms)
    for slot_index, release_ms in pairs(self.m_touched_leds) do
        if not Effect.c_lamp_lit_slots[slot_index] then
            local v
            local dt = time_now_ms - release_ms
            if dt < self.m_restore_ms then
                v = self.m_hsv[3] * dt // self.m_restore_ms
            else
                v = self.m_hsv[3]
                self.m_touched_leds[slot_index] = nil
            end

            fw.led_set_hsv(slot_index, self.m_hsv[1], self.m_hsv[2], v)
        end
    end

    fw.led_refresh()

    -- If m_touched_leds[] is empty, we stop the timer.
    if next(self.m_touched_leds) == nil then
        self:stop_timer()
    end
end



-------- Lamp
-- Lamp(lamp_id [, slot_index]) associates a lamp with an LED at slot_index. If
-- slot_index is not given, it automatically links to the LED during runtime that
-- initiated the lamp change. Custom behavior can optionally be defined through
-- on_lamp_active/inactive() when the lamp is turned on/off. Lamp can be even a super
-- class of keymap classes.
Lamp = Class()

Lamp.c_current_lamp_state = 0
Lamp.c_lamp_slots = {}  -- Lists of slot indices associated with each each lamp ID.

-- Check if the lamp corresponding to lamp_id is currently active.
function Lamp.is_lamp_active(lamp_id)
    return Lamp.c_current_lamp_state & (1 << (lamp_id - 1)) ~= 0
end

-- Note that each `Lamp()` instance is automatically stored in c_lamp_slots[], so
-- explicit global assignment is unnecessary.
function Lamp:init(lamp_id, slot_index)
    self.m_slot_index = slot_index
    self.m_next = Lamp.c_lamp_slots[lamp_id]
    Lamp.c_lamp_slots[lamp_id] = self
end

-- `Effect:on_lamp_active(slot_index)` handles LED lighting, whereas
-- `Lamp:on_lamp_active()` allows for custom behavior when the lamp turns on.
function Lamp:on_lamp_active() end
function Lamp:on_lamp_inactive() end



return ...,

-- Driver for keyboard indicator lamps, responsible for managing their on/off states.
-- This function, along with the keymap driver, is returned and passed to the next
-- script in the compilation sequence.
function(lamp_state)
    -- Compute the difference between the old and new states.
    lamp_state = lamp_state ~ Lamp.c_current_lamp_state
    -- Store the new state to c_current_lamp_state.
    Lamp.c_current_lamp_state = Lamp.c_current_lamp_state ~ lamp_state

    local lamp_id = 1  -- Starting from LAMP_NUMLOCK.
    while lamp_state ~= 0 do
        if lamp_state & 1 ~= 0 then
            local lamp = Lamp.c_lamp_slots[lamp_id]
            while lamp do
                -- Set .m_slot_index at runtime if not initialized, assuming SET_REPORT
                -- (lamp state change) arrives from the host rapidly while the
                -- triggering key is still being pressed.
                if not lamp.m_slot_index then
                    lamp.m_slot_index = Base.c_current_slot_index
                end

                if Lamp.is_lamp_active(lamp_id) then
                    Effect.c_lamp_lit_slots[lamp.m_slot_index] = true
                    Effect.c_active_effect:on_lamp_active(lamp.m_slot_index)
                    lamp:on_lamp_active()

                else
                    Effect.c_active_effect:on_lamp_inactive(lamp.m_slot_index)
                    Effect.c_lamp_lit_slots[lamp.m_slot_index] = nil
                    lamp:on_lamp_inactive()
                end

                lamp = lamp.m_next
            end
        end

        lamp_state = lamp_state >> 1
        lamp_id = lamp_id + 1
    end
end
