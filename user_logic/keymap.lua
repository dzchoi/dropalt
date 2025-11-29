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



-------- Custom keymaps
local FN    = Pseudo()

local QuickTap = TapOnPress|TapOnRelease|HoldIsTap

-- FN + ESC or holding ESC -> `
local mESC  = ModIf(FN, Lit("`"), TapHold(Lit("ESC"), Lit("`"), QuickTap))

-- FN + 1 or holding 1 -> F1, FN + 2 or holding 2 -> F2, ...
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

-- FN + Del -> Ins
local mDEL = ModIf(FN, Lit("INS"), Lit("DEL"))

-- FN + P -> PrtScr
local mP = ModIf(FN, Lit("PRTSCR"), Lit("P"))

-- FN + [ -> ScrLock
-- Most Linux Distros do not handle SCRLOCK but Windows does.
local mLBRAC = ModIf(FN, Lit("SCRLOCK"), Lit("["))
Lamp(LAMP_SCRLOCK, LED_BOTTOM_RIGHT)

-- FN + ] -> Break/Pause
local mRBRAC = ModIf(FN, Lit("PAUSE"), Lit("]"))

-- FN + Home or holding Home -> End
local mHOME = ModIf(FN, Lit("END"), TapHold(Lit("HOME"), Lit("END"), HoldIsTap))

-- Hold CapsLock -> FN
local tCAPSLOCK = TapHold(Lit("CAPSLOCK"), FN, HoldOnPress)
Lamp(LAMP_CAPSLOCK, LED_BOTTOM_LEFT)

-- FN + Left -> Home, FN + Right -> End, FN + Up -> PgUp, FN + Down -> PgDn
local mLEFT  = ModIf(FN, Lit("HOME"), Lit("LEFT"))
local mRIGHT = ModIf(FN, Lit("END"), Lit("RIGHT"))
local mUP    = ModIf(FN, Lit("PGUP"), Lit("UP"))
local mDOWN  = ModIf(FN, Lit("PGDN"), Lit("DOWN"))

-- FN + PgUp -> Volume Up
local mPGUP = ModIf(FN, Lit("VOLUP"), Lit("PGUP"))

-- FN + PgDn -> Volume Down, FN + holding DOWN -> Mute
local mPGDN = ModIf(FN, TapHold(Lit("VOLDN"), Lit("MUTE"), HoldIsTap), Lit("PGDN"))

-- FN + Tab -> fw.switchover()
local mTAB = ModIf(FN, Function(fw.switchover), Lit("TAB"))



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
    mESC, m1, m2, m3, m4, m5, m6, m7, m8, m9, m0, mMINUS, mEQUAL, "BKSP", mDEL,
    mTAB, "Q", "W", "E", "R", "T", "Y", "U", "I", "O", mP, mLBRAC, mRBRAC, "\\", mHOME,
    tCAPSLOCK, "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "ENTER", mPGUP,
    "LSHFT", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "RSHFT", mUP, mPGDN,
    "LCTRL", "LGUI", "LALT", "SPACE", FN, "RALT", mLEFT, mDOWN, mRIGHT
}

-- Register user-defined RGB effect.
-- https://stackoverflow.com/questions/21737613/image-of-hsv-color-wheel-for-opencv
local MildYellow  = 60  * HSV_HUE_STEPS // 360
Effect.c_active_effect = FingerTracer(8000, MildYellow, 255, 255)
