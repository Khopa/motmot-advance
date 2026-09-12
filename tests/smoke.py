#!/usr/bin/env python3
"""End-to-end smoke test: drives the ROM in mGBA through a Lua script.

It boots the ROM, walks title -> menu -> classic game, types an unknown word,
a valid wrong word, then the actual target word (read from RAM), checks the
win is recorded, visits the result and statistics screens, and takes a
screenshot at every step into tests/out/. A second run checks that the
statistics survived in SRAM.

usage: smoke.py [--mgba PATH] [--rom build/motmot.gba]
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
local SAVE = %(save)d      -- SaveData: magic(0..3) version(4) lang(5) sound(6) diff(7) played(8..9) won(10..11) lost(12..13) streak(14..15) best(16..17) marathon_best(70..73)
local RUN  = %(run)d
local MARATHON = %(marathon)d  -- difficulty(0) hp(1) hp_max(2) score(4..5)

local log = io.open(ROM_OUT .. "/smoke" .. RUN .. ".log", "w")
local function say(s) log:write(s, "\n"); log:flush(); console:log(s) end

local K = C.GBA_KEY
local layouts = {
  [0] = {"AZERTYUIOP", "QSDFGHJKLM", "\1WXCVBN\2"},   -- FR
  [1] = {"QWERTYUIOP", "ASDFGHJKL",  "\1ZXCVBNM\2"},  -- EN
}

local snd_reset, snd_poll, snd
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
  shot("00_language")
  say("save.played at boot = " .. u16(SAVE + 8))
  press(K.A); wait(5)                         -- keep the saved language
  shot("01_title")
  press(K.START); wait(5)
  shot("02_menu")
  press(K.A); wait(5)                         -- CLASSIC (preselected)
  local t = target()
  say("target = " .. t)
  shot("03_game")

  say("sound enabled = " .. (emu:read16(0x04000084) >> 7 & 1))
  snd_reset()
  type_word("AAAAA")
  say("sound while typing: sq1 = " .. snd.sq1 .. " noise = " .. snd.noise)
  snd_reset()
  press(K.START); wait(5)                     -- not in list -> buzzer
  say("sound on error: sq1 = " .. snd.sq1 .. " noise = " .. snd.noise)
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
  say("save.played = " .. u16(SAVE + 8) .. " won = " .. u16(SAVE + 10))
  press(K.B); wait(5)                         -- back to menu
  press(K.DOWN); press(K.DOWN); press(K.A); wait(5)   -- CLASSIC -> STATS
  shot("08_stats")
  press(K.B); wait(5)
  press(K.UP); press(K.UP); press(K.UP)       -- STATS -> LANGUAGE
  press(K.RIGHT); wait(5)                     -- switch language
  shot("09_menu_other_lang")
  press(K.DOWN); press(K.A); wait(5)          -- CLASSIC in the other language
  say("classic lang = " .. u8(GAME) .. " mode = " .. u8(GAME + 1))
  shot("10_game_other_lang")

  -- quitting asks for confirmation
  press(K.SELECT); wait(3)
  shot("11_quit_confirm")
  press(K.B); wait(3)                         -- no, stay
  say("still playing after cancelled quit: mode = " .. u8(GAME + 1))
  press(K.SELECT); wait(3); press(K.A); wait(5)   -- yes, quit -> menu (CLASSIC)

  -- lose a classic game: too-short message, then six valid wrong words
  press(K.A); wait(5)
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
  say("after loss: lost = " .. u16(SAVE + 12) .. " streak = " .. u16(SAVE + 14) .. " best = " .. u16(SAVE + 16))

  -- marathon, easy: find two words in a row, quit, check the high score
  press(K.B); wait(5)                         -- result -> menu (CLASSIC highlighted)
  press(K.DOWN)                               -- MARATHON
  while u8(SAVE + 7) ~= 0 do press(K.RIGHT) end   -- make sure difficulty = easy
  shot("15_menu_marathon")
  press(K.A); wait(5)
  say("marathon easy: hp = " .. u8(MARATHON + 1) .. "/" .. u8(MARATHON + 2) .. " score = " .. u16(MARATHON + 4))
  for i = 1, 2 do
    type_word(target()); press(K.START); wait(40)
    say("marathon word " .. i .. " found: score = " .. u16(MARATHON + 4) .. " hp = " .. u8(MARATHON + 1))
    if i == 1 then shot("16_marathon_word_found") end
    press(K.A); wait(5)                       -- skip the pause, next word
  end
  press(K.SELECT); wait(3); press(K.A); wait(5)   -- quit the run
  shot("17_marathon_result")
  say("marathon easy best = " .. u16(SAVE + 70))
  press(K.B); wait(5)                         -- menu

  -- marathon, hard: the third guess costs a life
  press(K.RIGHT); wait(3)                     -- difficulty -> hard
  press(K.A); wait(5)
  say("marathon hard: hp = " .. u8(MARATHON + 1) .. "/" .. u8(MARATHON + 2))
  local ht = target()
  local n = 0
  for _, w in ipairs(pool) do
    if n < 3 and w ~= ht then
      type_word(w); press(K.START); wait(60); n = n + 1
      say("marathon hard guess " .. n .. ": hp = " .. u8(MARATHON + 1))
    end
  end
  shot("18_marathon_hard")
  press(K.SELECT); wait(3); press(K.A); wait(5)
  say("marathon hard best = " .. u16(SAVE + 72))
  press(K.B); wait(5)
  say("done")
end)

-- sound monitor: highest envelope volume seen on square 1 / noise since reset
snd = { sq1 = 0, noise = 0 }
snd_reset = function() snd.sq1 = 0; snd.noise = 0 end
snd_poll = function()
  local v1 = emu:read16(0x04000062) >> 12      -- SOUND1CNT_H envelope volume
  local v4 = emu:read16(0x04000078) >> 12      -- SOUND4CNT_L envelope volume
  if v1 > snd.sq1 then snd.sq1 = v1 end
  if v4 > snd.noise then snd.noise = v4 end
end

callbacks:add("frame", function()
  snd_poll()
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
    ap.add_argument("--rom", default=os.path.join(ROOT, "build", "motmot.gba"))
    ap.add_argument("--keep-save", action="store_true", help="do not delete the .sav first")
    a = ap.parse_args()

    os.makedirs(OUT, exist_ok=True)
    elf = os.path.splitext(a.rom)[0] + ".elf"
    sav = os.path.splitext(a.rom)[0] + ".sav"
    syms = symbol_addresses(elf)
    for s in ("game", "kb", "save", "marathon"):
        if s not in syms:
            sys.exit(f"symbol {s} not found in {elf}")
    if not a.keep_save and os.path.exists(sav):
        os.remove(sav)

    failures = 0
    for run in (1, 2):
        script = os.path.join(ROOT, "build", f"smoke{run}.lua")
        with open(script, "w") as f:
            f.write(LUA % dict(out=OUT.replace("\\", "/"), game=syms["game"], kb=syms["kb"],
                               save=syms["save"], marathon=syms["marathon"], run=run))
        subprocess.run([a.mgba, "--script", script, a.rom], timeout=120)
        with open(os.path.join(OUT, f"smoke{run}.log")) as f:
            log = f.read()
        print(f"--- run {run} ---")
        print(log)
        boot = int(re.search(r"save.played at boot = (\d+)", log).group(1))
        boot_expect = 2 * (run - 1)           # each run plays one win + one loss
        checks = [
            ("no script error", "ERROR" not in log),
            ("script completed", "done" in log),
            (f"stats persisted (played at boot = {boot_expect})", boot == boot_expect),
            ("decoy accepted", "n_guesses after decoy = 1" in log),
            ("sound master enable", "sound enabled = 1" in log),
            ("key click plays on square 1", re.search(r"sound while typing: sq1 = [1-9]\d* noise = 0", log) is not None),
            ("buzzer uses square 1 and noise", re.search(r"sound on error: sq1 = [1-9]\d* noise = [1-9]", log) is not None),
            ("game won in 2", "status = 1 n_guesses = 2" in log),
            (f"stats updated (played = {2 * run - 1}, won = {run})", f"save.played = {2 * run - 1} won = {run}" in log),
            ("language switch applies to the game", f"classic lang = {run % 2} mode = 0" in log),
            ("quit needs confirmation", "still playing after cancelled quit: mode = 0" in log),
            ("game lost after 6 guesses", "lost: status = 2 n_guesses = 6" in log),
            ("marathon easy starts with 3 lives", "marathon easy: hp = 3/3 score = 0" in log),
            ("marathon easy: two words found", "marathon word 2 found: score = 2 hp = 3" in log),
            ("marathon easy high score saved", "marathon easy best = 2" in log),
            ("marathon hard starts with 5 lives", "marathon hard: hp = 5/5" in log),
            ("marathon hard: no loss on guesses 1-2", "marathon hard guess 2: hp = 5" in log),
            ("marathon hard: third guess costs a life", "marathon hard guess 3: hp = 4" in log),
            ("marathon hard high score saved", "marathon hard best = 0" in log),
            (f"loss recorded (lost = {run}, streak reset)", f"lost = {run} streak = 0 best = 1" in log),
        ]
        for name, ok in checks:
            print(("PASS " if ok else "FAIL ") + name)
            failures += not ok
    print("\n%d failure(s)" % failures if failures else "\nSmoke test passed.")
    return failures != 0


if __name__ == "__main__":
    sys.exit(main())
