#!/usr/bin/env python3
"""End-to-end smoke test: drives the ROM in mGBA through a Lua script.

It boots the ROM, walks title -> menu -> classic game, types an unknown word,
a valid wrong word, then the actual target word (read from RAM), checks the
win is recorded, visits the result and statistics screens, and takes a
screenshot at every step into tests/out/. A second run checks that the
statistics survived in SRAM.

usage: smoke.py [--mgba PATH] [--rom build/wordle.gba]
Needs an mGBA build with the --script option (0.11 dev or later).
"""
import argparse
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
OUT = os.path.join(HERE, "out")

NM = "C:/msys64/opt/devkitpro/devkitARM/bin/arm-none-eabi-nm.exe"


def symbol_addresses(elf):
    out = subprocess.check_output([NM, elf], text=True)
    syms = {}
    for line in out.splitlines():
        m = re.match(r"([0-9a-fA-F]+) [BbDdRr] (\w+)$", line)
        if m:
            syms[m.group(2)] = int(m.group(1), 16)
    return syms


LUA = r"""
local ROM_OUT = "%(out)s"
local GAME = %(game)d      -- GameState: lang(0) mode(1) status(2) target(3..7) n_guesses(8)
local KB   = %(kb)d        -- KbCursor: row, col
local SAVE = %(save)d      -- SaveData: magic(0..3) version(4) lang(5) played(6..7) won(8..9)
local RUN  = %(run)d

local log = io.open(ROM_OUT .. "/smoke" .. RUN .. ".log", "w")
local function say(s) log:write(s, "\n"); log:flush(); console:log(s) end

local K = C.GBA_KEY
local layouts = {
  [0] = {"AZERTYUIOP", "QSDFGHJKLM", "\1WXCVBN\2"},   -- FR
  [1] = {"QWERTYUIOP", "ASDFGHJKL",  "\1ZXCVBNM\2"},  -- EN
}

local co = coroutine.create(function()
  local function wait(n) for _ = 1, n do coroutine.yield() end end
  local function press(key, hold)
    emu:addKey(key); wait(hold or 2); emu:clearKey(key); wait(3)
  end
  local function shot(name)
    emu:screenshot(ROM_OUT .. "/" .. name .. ".png"); say("screenshot " .. name)
  end
  local function u8(a) return emu:read8(a) end
  local function u16(a) return emu:read16(a) end
  local function target()
    local t = ""
    for i = 0, 4 do t = t .. string.char(u8(GAME + 3 + i)) end
    return t
  end
  local function find_key(layout, ch)
    for r = 1, 3 do
      local c = string.find(layout[r], ch, 1, true)
      if c then return r - 1, c - 1 end
    end
    error("key not in layout: " .. ch)
  end
  local function goto_key(ch)
    local layout = layouts[u8(GAME)]
    local tr, tc = find_key(layout, ch)
    for _ = 1, 16 do
      local r, c = u8(KB), u8(KB + 1)
      if r == tr and c == tc then return end
      if r ~= tr then press((tr - r) %% 3 == 1 and K.DOWN or K.UP)
      else
        local n = #layout[r + 1]
        press((tc - c) %% n <= n / 2 and K.RIGHT or K.LEFT)
      end
    end
    error("could not reach key " .. ch)
  end
  local function type_word(w)
    for i = 1, #w do goto_key(w:sub(i, i)); press(K.A) end
  end

  wait(30)
  shot("01_title")
  say("save.played at boot = " .. u16(SAVE + 6))
  press(K.START); wait(5)
  shot("02_language")
  press(K.A); wait(5)                         -- keep the saved language
  shot("02_menu")
  press(K.A); wait(5)                         -- CLASSIC (preselected)
  local t = target()
  say("target = " .. t)
  shot("03_game")

  type_word("AAAAA"); press(K.START); wait(5)  -- not in list
  shot("04_unknown_word")
  for _ = 1, 5 do press(K.B) end

  local decoys = { [0] = {"TERRE", "PORTE"}, [1] = {"CRANE", "SLATE"} }
  local d = decoys[u8(GAME)]
  local decoy = (t == d[1]) and d[2] or d[1]
  type_word(decoy); press(K.START); wait(45)  -- valid wrong word + reveal
  say("n_guesses after decoy = " .. u8(GAME + 8))
  shot("05_after_guess")

  type_word(t); press(K.START); wait(45)
  say("status = " .. u8(GAME + 2) .. " n_guesses = " .. u8(GAME + 8))
  shot("06_won")
  press(K.A); wait(5)
  shot("07_result")
  say("save.played = " .. u16(SAVE + 6) .. " won = " .. u16(SAVE + 8))
  press(K.B); wait(5)                         -- back to menu
  press(K.DOWN); press(K.DOWN); press(K.A); wait(5)   -- CLASSIC -> STATS
  shot("08_stats")
  press(K.B); wait(5)
  press(K.RIGHT); wait(5)                     -- switch language -> EN
  shot("09_menu_en")
  press(K.UP); press(K.A); wait(5)            -- CHALLENGE (English: started there in run 1)
  say("challenge lang = " .. u8(GAME) .. " mode = " .. u8(GAME + 1) .. " n_guesses at entry = " .. u8(GAME + 8))
  shot("10_challenge_en")
  local ct = target()
  local cd = (ct == "CRANE") and "SLATE" or "CRANE"
  type_word(cd); press(K.START); wait(45)
  say("challenge n_guesses after guess = " .. u8(GAME + 8))
  press(K.SELECT); wait(5)                    -- quit to menu, progress saved
  press(K.A); wait(5)                         -- re-enter the challenge
  say("challenge n_guesses after resume = " .. u8(GAME + 8))
  shot("11_challenge_resumed")

  -- lose a classic game: too-short message, then six valid wrong words
  press(K.SELECT); wait(5)
  press(K.UP); press(K.A); wait(5)            -- menu item CHALLENGE -> CLASSIC
  local lt = target()
  local pool = ({ [0] = {"TERRE", "PORTE", "TABLE", "CHIEN", "ROUGE", "BLANC", "MONDE"},
                  [1] = {"SLATE", "CRANE", "ABBEY", "ALLEY", "APPLE", "EERIE", "LEVEL"} })[u8(GAME)]
  type_word("AB"); press(K.START); wait(3)
  shot("12_too_short")
  press(K.B); press(K.B)
  local n = 0
  for _, w in ipairs(pool) do
    if n < 6 and w ~= lt then
      type_word(w); press(K.START); wait(45); n = n + 1
    end
  end
  say("lost: status = " .. u8(GAME + 2) .. " n_guesses = " .. u8(GAME + 8))
  shot("13_lost")
  press(K.A); wait(5)
  shot("14_result_lost")
  say("after loss: lost = " .. u16(SAVE + 10) .. " streak = " .. u16(SAVE + 12) .. " best = " .. u16(SAVE + 14))
  say("done")
end)

callbacks:add("frame", function()
  if coroutine.status(co) ~= "dead" then
    local ok, err = coroutine.resume(co)
    if not ok then say("ERROR " .. tostring(err)); os.exit(1) end
  else
    log:close(); os.exit(0)
  end
end)
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mgba", default="C:/Tools/mgba-nightly/mGBA.exe")
    ap.add_argument("--rom", default=os.path.join(ROOT, "build", "wordle.gba"))
    ap.add_argument("--keep-save", action="store_true", help="do not delete the .sav first")
    a = ap.parse_args()

    os.makedirs(OUT, exist_ok=True)
    elf = os.path.splitext(a.rom)[0] + ".elf"
    sav = os.path.splitext(a.rom)[0] + ".sav"
    syms = symbol_addresses(elf)
    for s in ("game", "kb", "save"):
        if s not in syms:
            sys.exit(f"symbol {s} not found in {elf}")
    if not a.keep_save and os.path.exists(sav):
        os.remove(sav)

    failures = 0
    for run in (1, 2):
        script = os.path.join(ROOT, "build", f"smoke{run}.lua")
        with open(script, "w") as f:
            f.write(LUA % dict(out=OUT.replace("\\", "/"), game=syms["game"], kb=syms["kb"],
                               save=syms["save"], run=run))
        subprocess.run([a.mgba, "--script", script, a.rom], timeout=120)
        with open(os.path.join(OUT, f"smoke{run}.log")) as f:
            log = f.read()
        print(f"--- run {run} ---")
        print(log)
        boot = int(re.search(r"save.played at boot = (\d+)", log).group(1))
        expect = run - 1                      # challenges/guesses carried over
        boot_expect = 2 * (run - 1)           # each run plays one win + one loss
        checks = [
            ("no script error", "ERROR" not in log),
            ("script completed", "done" in log),
            (f"stats persisted (played at boot = {boot_expect})", boot == boot_expect),
            ("decoy accepted", "n_guesses after decoy = 1" in log),
            ("game won in 2", "status = 1 n_guesses = 2" in log),
            (f"stats updated (played = {2 * run - 1}, won = {run})", f"save.played = {2 * run - 1} won = {run}" in log),
            (f"challenge restored from SRAM ({expect} guess)", f"n_guesses at entry = {expect}" in log),
            (f"challenge guess saved ({run})", f"after guess = {run}" in log),
            (f"challenge resumed after quit ({run})", f"after resume = {run}" in log),
            ("game lost after 6 guesses", "lost: status = 2 n_guesses = 6" in log),
            (f"loss recorded (lost = {run}, streak reset)", f"lost = {run} streak = 0 best = 1" in log),
        ]
        for name, ok in checks:
            print(("PASS " if ok else "FAIL ") + name)
            failures += not ok
    print("\n%d failure(s)" % failures if failures else "\nSmoke test passed.")
    return failures != 0


if __name__ == "__main__":
    sys.exit(main())
