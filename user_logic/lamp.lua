-------- Lamp
-- Associates an indicator lamp with an LED.
--   - Lamp(lamp_id, slot_index): directly binds the lamp to the given slot index.
--   - Lamp(lamp_id, keymap): binds the lamp to the slot that will later be assigned to
--     the keymap.
--     Note: In this case, the keymap is temporarily extended with a `_lamp` field
--     referencing this Lamp instance. Once the keymap is assigned to a slot,
--     `Lamp.m_slot_index` must be updated to match the assigned slot index. See
--     `layout()` for an example.
--
-- Custom behavior can optionally be defined through on_lamp_active/inactive(), which
-- is invoked when the lamp is turned on or off.
--
-- Note: unlike keymap classes, there's no need to save Lamp class instances in the
-- global environment to survive garbage collection, because each instance is
-- automatically stored in the class variable `Lamp.c_lamp_slots[]`.
Lamp = Class()

Lamp.c_current_lamp_state = 0
Lamp.c_lamp_slots = {}  -- Lists of slot indices associated with each each lamp ID.

function Lamp:init(lamp_id, slot_index_or_keymap)
    if type(slot_index_or_keymap) == "number" then
        self.m_slot_index = slot_index_or_keymap
    else
        slot_index_or_keymap._lamp = self
    end
    self.m_next = Lamp.c_lamp_slots[lamp_id]
    Lamp.c_lamp_slots[lamp_id] = self
end

-- Checks if the lamp corresponding to lamp_id is currently active.
function Lamp.is_lamp_active(lamp_id)
    return Lamp.c_current_lamp_state & (1 << (lamp_id - 1)) ~= 0
end

-- `Effect:on_lamp_active(slot_index)` handles the physical LED lighting, whereas
-- `Lamp:on_lamp_active()` allows for additional behavior for a user-defined Lamp when
-- the lamp turns on.
function Lamp:on_lamp_active() end
function Lamp:on_lamp_inactive() end
