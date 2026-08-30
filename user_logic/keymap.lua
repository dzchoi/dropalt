-- Default key mapping definitions for the "keymap" module

-- 🚨 Note: Any error in the scripts will cause a crash, either during module loading at
-- boot or when executing a keymap triggered by pressing a key. For example, calling
-- `non_existent_function()` will crash and produce an error message in `dalua -d` or
-- `dfu-util -a0 -U ...`. Fix the script and re‑download it to recover.



-------- Custom keymaps
local Pseudo = Base  -- Base can be used standalone.
local FN     = Pseudo()

local QuickTap = TapOnPress|TapOnRelease|HoldIsTap

-- FN + ESC or holding ESC -> `
local mESC  = If(FN, "`", TapHold("ESC", "`", QuickTap))

-- FN + 1 or holding 1 -> F1, FN + 2 or holding 2 -> F2, ...
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

-- FN + Del -> Ins
local mDEL = If(FN, "INS", "DEL")

-- FN + P -> PrtScr
local mP = If(FN, "PRTSCR", "P")

-- FN + [ -> ScrLock
-- Most Linux Distros do not handle SCRLOCK but Windows does.
local mLBRAC = If(FN, "SCRLOCK", "[")
Lamp(LAMP_SCRLOCK, LED_BOTTOM_RIGHT)

-- FN + ] -> Break/Pause
local mRBRAC = If(FN, "PAUSE", "]")

-- FN + Home or holding Home -> End
local mHOME = If(FN, "END", TapHold("HOME", "END", HoldIsTap))

-- Hold CapsLock -> FN
local tCAPSLOCK = TapHold("CAPSLOCK", FN, HoldOnPress)
Lamp(LAMP_CAPSLOCK, LED_BOTTOM_LEFT)

-- FN + Left -> Home, FN + Right -> End, FN + Up -> PgUp, FN + Down -> PgDn
local mLEFT  = If(FN, "HOME", "LEFT")
local mRIGHT = If(FN, "END", "RIGHT")
local mUP    = If(FN, "PGUP", "UP")
local mDOWN  = If(FN, "PGDN", "DOWN")

-- FN + PgUp -> Volume Up
local mPGUP = If(FN, "VOLUP", "PGUP")

-- FN + PgDn -> Volume Down, FN + holding DOWN -> Mute
local mPGDN = If(FN, TapHold("VOLDN", "MUTE", HoldIsTap), "PGDN")

-- FN + Tab -> fw.switchover()
local mTAB = If(FN, fw.switchover, "TAB")



-- Register user-defined keymaps.
Base.c_keymap_table = {
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
