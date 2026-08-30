-- Advanced key mapping example for the "keymap" module

-- 🚨 Note: Any error in the scripts will cause a crash, either during module loading at
-- boot or when executing a keymap triggered by pressing a key. For example, calling
-- `non_existent_function()` will crash and produce an error message in `dalua -d` or
-- `dfu-util -a0 -U ...`. Fix the script and re‑download it to recover.



-------- LampJiggler
-- A custom Lamp that periodically taps a key while the lamp is active.
-- Note that each `LampJiggler()` instance is automatically stored in c_lamp_slots[], so
-- explicit global assignment is unnecessary.
local LampJiggler = Class(Lamp, Timer)

function LampJiggler:init(lamp_id, slot_index_or_keymap, jiggler_keymap, jiggle_period_ms)
    Lamp.init(self, lamp_id, slot_index_or_keymap)
    Timer.init(self)
    self.m_jiggler_keymap = Base.to_keymap(jiggler_keymap)
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
local Pseudo = Base  -- Base can be used standalone.
local FN     = Pseudo()
local FN2    = Pseudo()

-- FN + ` -> fw.dfu_mode(), FN + holding ` -> Power
local mBKTK = If(FN,
    TapHold(fw.dfu_mode, "POWER", HoldIsTap), "`")

-- FN + 1 or holding 1 -> F1, FN + 2 or holding 2 -> F2, ...
local QuickTap = TapOnPress|TapOnRelease|HoldIsTap
local m1 = If(FN, "F1", TapHold("1", "F1", QuickTap))
local m2 = If(FN, "F2", TapHold("2", "F2", QuickTap))
local m3 = If(FN, "F3", TapHold("3", "F3", QuickTap))
local m4 = If(FN, "F4", TapHold("4", "F4", QuickTap))
local m5 = If(FN, "F5", TapHold("5", "F5", QuickTap))
local m6 = If(FN, "F6", TapHold("6", "F6", QuickTap))
local m7 = If(FN, "F7", TapHold("7", "F7", QuickTap))
local m8 = If(FN, "F8", TapHold("8", "F8", QuickTap))
local m9 = If(FN, "F9", TapHold("9", "F9", QuickTap))
local m0 = If(FN, "F10", TapHold("0", "F10", QuickTap))
local mMINUS = If(FN, "F11", TapHold("-", "F11", QuickTap))
local mEQUAL = If(FN, "F12", TapHold("=", "F12", QuickTap))

-- FN + BkSp -> Del
local mBKSP = If(FN, "DEL", "BKSP")

-- Hold Tab -> FN2, FN + Tab -> fw.switchover()
local mTAB = If(FN,
    -- Directly executing fw.switchover() is safe here because the modifier is a
    -- non-physical key (FN). However, if it were e.g. CTRL, fw.switchover() should be
    -- called through fw.execute_later().
    -- function() fw.execute_later(fw.switchover) end,
    fw.switchover,
    TapHold("TAB", FN2, HoldOnPress))

-- FN + P -> PrtScr
local mP = If(FN, "PRTSCR", "P")

-- FN + [ -> ScrLock
-- Most Linux Distros do not handle SCRLOCK but Windows does.
local mLBRAC = If(FN, "SCRLOCK", "[")
-- Periodically taps RSHFT every 5 minutes while SCRLOCK lamp is lit.
LampJiggler(LAMP_SCRLOCK, LED_BOTTOM_RIGHT, "RSHFT", 299000)  -- 4 min 59 sec

-- FN + ] -> Break/Pause
local mRBRAC = If(FN, "PAUSE", "]")

-- Tap FN -> Esc
local tFN = TapHold("ESC", FN, HoldOnPress)

-- FN + H/J/K/L -> arrow keys, FN2 + H/J/K/L -> Home/PgDn/PgUp/End
local mH = If(FN, "LEFT", If(FN2, "HOME", "H"))
local mJ = If(FN, "DOWN", If(FN2, "PGDN", "J"))
local mK = If(FN, "UP", If(FN2, "PGUP", "K"))
local mL = If(FN, "RIGHT", If(FN2, "END", "L"))

-- Hold Enter -> FN
local tENTER = TapHold("ENTER", FN, HoldOnPress)

-- Tap Space -> Space, Hold Space -> Rshft, Tap + Tap + Hold Space -> Space
local tSPACE = TapHold("SPACE", "RSHFT", HoldOnRelease|QuickRelease)
tSPACE = TapSeq(tSPACE, tSPACE, "SPACE")

-- Lshft w/tSPACE (not w/Rshft) -> Space
-- Double-tap Lshft -> CapsLock, Tap Lshft (when CapsLock on) -> CapsLock
local mLSHFT = If(tSPACE, "SPACE",
    If(function() return Lamp.is_lamp_active(LAMP_CAPSLOCK) end,
        "CAPSLOCK", TapSeq("LSHFT", "CAPSLOCK")) )
Lamp(LAMP_CAPSLOCK, LED_BOTTOM_LEFT)

-- Tap Rshft -> Ins
local tRSHFT = TapHold("INS", "RSHFT", HoldOnPress|QuickRelease)



-- Register user-defined keymaps.
Base.c_keymap_table = {
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
