-- Key mapping definitions for the "keymap" module

-- 🚨 Note: Any error in the scripts will cause a crash, either during module loading at
-- boot or when executing a keymap triggered by pressing a key. For example, calling
-- `non_existent_function()` will crash and produce an error message in `dalua -d` or
-- `dfu-util -a0 -U ...`. Fix the script and re‑download it to recover.



-------- Lit()
-- Memoized version of Lit() to avoid creating multiple instances with the same keyname.
-- This function is used only during load time and discarded afterward, since it is
-- defined locally and the cache[] array as well.
-- Note that this function is used when defining a keymap instance (e.g., mHome =
-- ModIf(FN, Lit("END"), Lit("HOME"))). It executes at compile time, and because it is
-- declared local, it is removed after loading the "keymap" module.
local cache = {}
local function Lit(keyname)
    if not cache[keyname] then
        cache[keyname] = _ENV.Lit(keyname)  -- _ENV.Lit() is the global Lit().
    end
    return cache[keyname]
end

-------- LampJiggler
-- A custom Lamp that periodically taps a key while the lamp is active.
-- Note that each `LampJiggler()` instance is automatically stored in c_lamp_slots[], so
-- explicit global assignment is unnecessary.
local LampJiggler = Class(Lamp, Timer)

function LampJiggler:init(lamp_id, slot_index_or_keymap, jiggler_keymap, jiggle_period_ms)
    Lamp.init(self, lamp_id, slot_index_or_keymap)
    Timer.init(self)
    self.m_jiggler_keymap = jiggler_keymap
    self.m_jiggle_period_ms = jiggle_period_ms
end

function LampJiggler:on_lamp_active()
    self:on_timeout()
    self:start_timer(self.m_jiggle_period_ms, true)
end

function LampJiggler:on_lamp_inactive()
    self:stop_timer()
end

function LampJiggler:on_timeout()
    fw.execute_later(
        function()
            self.m_jiggler_keymap:_press()
            self.m_jiggler_keymap:_release()
        end)
end



-------- Custom keymaps
local FN    = Pseudo()
local FN2   = Pseudo()

-- FN + ` -> fw.dfu_mode(), FN + holding ` -> Power
local mBKTK = ModIf(FN,
    TapHold(Function(fw.dfu_mode), Lit("POWER"), HoldIsTap), Lit("`"))

-- FN + 1 or holding 1 -> F1, FN + 2 or holding 2 -> F2, ...
local QuickTap = TapOnPress|TapOnRelease|HoldIsTap
local m1 = ModIf(FN, Lit("F1"), TapHold(Lit("1"), Lit("F1"), QuickTap))
local m2 = ModIf(FN, Lit("F2"), TapHold(Lit("2"), Lit("F2"), QuickTap))
local m3 = ModIf(FN, Lit("F3"), TapHold(Lit("3"), Lit("F3"), QuickTap))
local m4 = ModIf(FN, Lit("F4"), TapHold(Lit("4"), Lit("F4"), QuickTap))
local m5 = ModIf(FN, Lit("F5"), TapHold(Lit("5"), Lit("F5"), QuickTap))
local m6 = ModIf(FN, Lit("F6"), TapHold(Lit("6"), Lit("F6"), QuickTap))
local m7 = ModIf(FN, Lit("F7"), TapHold(Lit("7"), Lit("F7"), QuickTap))
local m8 = ModIf(FN, Lit("F8"), TapHold(Lit("8"), Lit("F8"), QuickTap))
local m9 = ModIf(FN, Lit("F9"), TapHold(Lit("9"), Lit("F9"), QuickTap))
local m0 = ModIf(FN, Lit("F10"), TapHold(Lit("0"), Lit("F10"), QuickTap))
local mMINUS = ModIf(FN, Lit("F11"), TapHold(Lit("-"), Lit("F11"), QuickTap))
local mEQUAL = ModIf(FN, Lit("F12"), TapHold(Lit("="), Lit("F12"), QuickTap))

-- FN + BkSp -> Del
local mBKSP = ModIf(FN, Lit("DEL"), Lit("BKSP"))

-- Hold Tab -> FN2, FN + Tab -> fw.switchover()
local mTAB = ModIf(FN,
    -- Directly executing fw.switchover() is safe here because the modifier is a
    -- non-physical key (FN). However, if it were e.g. CTRL, fw.switchover() should be
    -- called through fw.execute_later().
    -- Function(function() fw.execute_later(fw.switchover) end),
    Function(fw.switchover),
    TapHold(Lit("TAB"), FN2, HoldOnPress))

-- FN + P -> PrtScr
local mP = ModIf(FN, Lit("PRTSCR"), Lit("P"))

-- FN + [ -> ScrLock
-- Most Linux Distros do not handle SCRLOCK but Windows does.
local mLBRAC = ModIf(FN, Lit("SCRLOCK"), Lit("["))
-- Periodically taps RSHFT every 5 minutes while SCRLOCK lamp is lit.
LampJiggler(LAMP_SCRLOCK, LED_BOTTOM_RIGHT, Lit("RSHFT"), 299000)  -- 4 min 59 sec

-- FN + ] -> Break/Pause
local mRBRAC = ModIf(FN, Lit("PAUSE"), Lit("]"))

-- Tap FN -> Esc
local tFN = TapHold(Lit("ESC"), FN, HoldOnPress)

-- FN + H/J/K/L -> arrow keys, FN2 + H/J/K/L -> Home/PgDn/PgUp/End
local mH = ModIf(FN, Lit("LEFT"), ModIf(FN2, Lit("HOME"), Lit("H")))
local mJ = ModIf(FN, Lit("DOWN"), ModIf(FN2, Lit("PGDN"), Lit("J")))
local mK = ModIf(FN, Lit("UP"), ModIf(FN2, Lit("PGUP"), Lit("K")))
local mL = ModIf(FN, Lit("RIGHT"), ModIf(FN2, Lit("END"), Lit("L")))

-- Hold Enter -> FN
local tENTER = TapHold(Lit("ENTER"), FN, HoldOnPress)

-- Tap Space -> Space, Hold Space -> Rshft, Tap + Tap + Hold Space -> Space
local tSPACE = TapHold(Lit("SPACE"), Lit("RSHFT"), HoldOnRelease|QuickRelease)
tSPACE = TapSeq(tSPACE, tSPACE, Lit("SPACE"))

-- Lshft w/tSPACE (not w/Rshft) -> Space
-- Double-tap Lshft -> CapsLock, Tap Lshft (when CapsLock on) -> CapsLock
local mLSHFT = ModIf(tSPACE, Lit("SPACE"),
    ModIf(Predicate(function() return Lamp.is_lamp_active(LAMP_CAPSLOCK) end),
        Lit("CAPSLOCK"), TapSeq(Lit("LSHFT"), Lit("CAPSLOCK"))) )
Lamp(LAMP_CAPSLOCK, LED_BOTTOM_LEFT)

-- Tap Rshft -> Ins
local tRSHFT = TapHold(Lit("INS"), Lit("RSHFT"), HoldOnPress|QuickRelease)



-------- Generate keymap table from the user-defined layout.
local function layout(keymaps)
    assert( #keymaps == KEY_LED_COUNT )
    for i, keymap in ipairs(keymaps) do
        if type(keymap) == "string" then
            keymaps[i] = Lit(keymap)
        else
            -- It should be an instance of Base.
            assert( keymap._press, "keymaps["..i.."] not valid" )
            -- If the keymap is associated with a lamp, link the lamp to the slot
            -- currently occupied by the keymap.
            if keymap._lamp then
                keymap._lamp.m_slot_index = i
                keymap._lamp = nil
            end
        end
    end

    return keymaps
end

-- Register user-defined keymaps.
Base.c_keymap_table = layout {
    mBKTK, m1, m2, m3, m4, m5, m6, m7, m8, m9, m0, mMINUS, mEQUAL, mBKSP, "DEL",
    mTAB, "Q", "W", "E", "R", "T", "Y", "U", "I", "O", mP, mLBRAC, mRBRAC, "\\", "HOME",
    tFN, "A", "S", "D", "F", "G", mH, mJ, mK, mL, ";", "'", tENTER, "PGUP",
    mLSHFT, "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", tRSHFT, "UP", "PGDN",
    "LALT", "LGUI", "LCTRL", tSPACE, "RCTRL", "RALT", "LEFT", "DOWN", "RIGHT"
}

-- Register user-defined RGB effect.
-- https://stackoverflow.com/questions/21737613/image-of-hsv-color-wheel-for-opencv
local MildYellow  = 60  * HSV_HUE_STEPS // 360
-- local Orange      = 30  * HSV_HUE_STEPS // 360
-- local SpringGreen = 90  * HSV_HUE_STEPS // 360
-- local MildGreen   = 120 * HSV_HUE_STEPS // 360
-- Effect.c_active_effect = Solid(SpringGreen, 255, 255)
Effect.c_active_effect = FingerTracer(8000, MildYellow, 255, 255)

-- For debugging, expose _ENV so we can inspect module entries:
-- $ dalua -e 'for k,v in pairs(env) do; print(k, v); end' |sort
-- _G.env = _ENV
