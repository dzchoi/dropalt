-- These variables are captured as upvalues in the driver functions below.
local Base = Base
local Defer = Defer
local Effect = Effect
local Lamp = Lamp

-- Core keymap driver (engine) responsible for processing key events and dispatching
-- them to user-defined mappings.
local function handle_key_event(slot_index, is_press)
    Base.c_current_slot_index = slot_index
    local press_or_release = is_press and "press" or "release"

    local deferrer = Defer.c_owner
    if deferrer then
        -- In defer mode, if the event targets the deferrer, execute it immediately.
        if slot_index == Defer.c_slot_index then
            fw.log("Map: [%d] handle deferrer %s", slot_index, press_or_release)

        -- In defer mode, if the event targets a slot other than the deferrer's, notify
        -- the deferrer first.
        else
            fw.log("Map: [%d] handle other %s @[%d]",
                Defer.c_slot_index, press_or_release, slot_index)
            if not deferrer:_other_event(is_press) then
                return
            end

            -- If the deferrer has opted not to defer this event by returning a non-nil,
            -- execute it immediately.
            fw.log("Map: [%d] execute immediate %s", slot_index, press_or_release)
        end

        -- Note: Those two cases of executing events immediately during defer mode can
        -- disrupt the original key event order. See notes in TapHold.

    -- Execute the event if in normal mode.
    else
        fw.log("Map: [%d] handle %s", slot_index, press_or_release)
    end

    if is_press then
        if not Base.c_keymap_table[slot_index]:is_pressed() then
            Effect.c_active_effect:_press(slot_index)
        end
        Base.c_keymap_table[slot_index]:_press()
    else
        if Base.c_keymap_table[slot_index]:is_pressed() then
            Effect.c_active_effect:_release(slot_index)
        end
        Base.c_keymap_table[slot_index]:_release()
    end

    -- If we were in defer mode, remove the latest deferred event from the event queue,
    -- since it has already executed - either on the deferrer or on another slot.
    -- Note: This deferred event (just peeked) should have .slot_index equal to the
    -- current slot_index. See the while-loop in main_thread.
    if deferrer then
        Defer.remove_last()
    end
end

-- Driver for keyboard indicator lamps, responsible for managing their on/off states.
local function handle_lamp_state(lamp_state)
    -- Compute the difference between the old and new states.
    lamp_state = lamp_state ~ Lamp.c_current_lamp_state
    -- Store the new state to c_current_lamp_state.
    Lamp.c_current_lamp_state = Lamp.c_current_lamp_state ~ lamp_state

    local lamp_id = 1  -- Starting from LAMP_NUMLOCK.
    while lamp_state ~= 0 do
        if lamp_state & 1 ~= 0 then
            local lamp = Lamp.c_lamp_slots[lamp_id]
            while lamp do
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



-- Return a module table with the keymap and lamp drivers.
return {handle_key_event, handle_lamp_state}
