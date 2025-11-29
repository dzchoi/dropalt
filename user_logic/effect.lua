-------- Configuration
-- Rate for updating RGB leds to update effects. 17 ms corresponds to ~60 fps.
RGB_UPDATE_PERIOD_MS = 17

-------- LED constants from led_conf.h
KEY_LED_COUNT     = 67  -- Key LEDs: slot_index (a.k.a. led_index) from 1 to 67
LED_BOTTOM_LEFT   = 68  -- Underglow LEDs: slot_index >= 68
LED_BOTTOM_RIGHT  = 82
LED_TOP_RIGHT     = 87
LED_TOP_LEFT      = 101
ALL_LED_COUNT     = 105
HSV_HUE_STEPS     = 0x600  -- Number of hue steps for HSV color space

-------- Lamp ID
LAMP_NUMLOCK      = 1
LAMP_CAPSLOCK     = 2
LAMP_SCRLOCK      = 3
-- LAMP_COMPOSE      = 4
-- LAMP_KANA         = 5



-------- Effect
-- Effect: Base class for keyboard LED animation schemes.
Effect = Class()

-- Effect.c_active_effect is set to Effect to disable LED activity by default. It can
-- later be overriden by a subclass of Effect.
-- Note: Do not override if ENABLE_RGB_LED is false.
Effect.c_active_effect = Effect

-- Effect.c_lamp_lit_slots tracks slot indices that should be excluded from Effect
-- updates. Each entry can hold one of three states:
--   - nil:   lamp is inactive
--   - false: lamp is inactive, but its activator key has been pressed
--   - true:  lamp is active
Effect.c_lamp_lit_slots = {}

-- State transitions:
-- The Activate/Inactivate event (lamp-state change) may be delivered either before or
-- after the corresponding key Release event, depending on the host OS.
--
-- On Windows:
-- Events: Press -> Activate -> Release  ...  Press -> Inactivate -> Release
-- State:  false*     true       true    ...   true       nil          nil
--
-- On Linux:
-- Events: Press -> Release -> Activate  ...  Press -> Release -> Inactivate
-- State:  false*    false*      true    ...   true      true        nil
--
-- * Only these three cases trigger on_press/release().

-- Effect:init() is invoked when an Effect instance is created during keymap module
-- loading. The effect becomes visible upon the USB resume event, which enables GCR
-- across all LEDs.
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

-------- "Solid Color"
Solid = Class(Effect)

-- Solid color LEDs; they will be turned on on USB resume.
function Solid:init(...)
    Effect.init(self)

    self.m_hsv = {...}
    for slot_index = 1, KEY_LED_COUNT do
        fw.led_set_hsv(slot_index, ...)
        -- Or fw.led_set_hsv(slot_index, fw.unpack(self.m_hsv))
    end
    fw.led_refresh()
end

function Solid:on_lamp_active(slot_index)
    -- If slot_index <= KEY_LED_COUNT, turn it off. Otherwise, turn it on.
    fw.led_set_hsv(slot_index, self.m_hsv[1], self.m_hsv[2],
        slot_index <= KEY_LED_COUNT and 0 or self.m_hsv[3])
    fw.led_refresh()
end

function Solid:on_lamp_inactive(slot_index)
    fw.led_set_hsv(slot_index, self.m_hsv[1], self.m_hsv[2],
        slot_index <= KEY_LED_COUNT and self.m_hsv[3] or 0)
    fw.led_refresh()
end

-------- "Finger Tracer"
FingerTracer = Class(Solid, Timer)

function FingerTracer:init(restore_ms, ...)
    Solid.init(self, ...)
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
