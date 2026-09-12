-- MotMot Advance — Lua helper library for the mGBA scenarios.
--
-- run.py prepends a CFG table (symbol addresses, struct offsets, output
-- directory, scenario name) and this file to every scenario, then runs the
-- result with `mGBA --script`. A scenario is a plain function passed to
-- T.run(); it runs as a coroutine that yields once per emulated frame.
--
--   T.run(function()
--     T.boot(T.LANG.FR)                -- power on, pick the language, reach the menu
--     T.menu_start(T.MENU.CLASSIC)     -- start a classic game
--     T.type_word("TERRE"); T.submit() -- play
--     T.check_eq(T.game().n_guesses, 1, "guess accepted")
--   end)
--
-- Other entry points: T.start_marathon(diff), T.start_time_attack(len_idx),
-- T.set_language(lang), T.set_sound(on) (through the options screen).
--
-- Every T.check* call writes a PASS / FAIL line to the scenario log; run.py
-- turns those into the summary. Reading the ROM's memory (game state, save
-- data, cursor) is the primary way to assert; screenshots are for humans.

T = {}
local K = C.GBA_KEY
T.K = K

-- --- constants mirrored from the C enums (main.c / game_state.h) ------------
T.SCREEN = { TITLE = 0, LANG = 1, MENU = 2, OPTIONS = 3, RECORDS = 4, MODE_SELECT = 5,
             GAME = 6, RESULT = 7, MARATHON_RESULT = 8, TA_RESULT = 9, STATS = 10 }
T.MENU   = { CLASSIC = 0, MARATHON = 1, TIME_ATTACK = 2, RECORDS = 3, STATS = 4, OPTIONS = 5, COUNT = 6 }
T.OPT    = { LANGUAGE = 0, SOUND = 1, COUNT = 2 }
T.LANG   = { FR = 0, EN = 1 }
T.MODE   = { CLASSIC = 0, MARATHON = 1, TIME_ATTACK = 2 }
T.TA_WORDS = { [0] = 5, [1] = 10, [2] = 15 }          -- ta_word_counts
T.TA_PENALTY = 30 * 60
T.STATUS = { PLAYING = 0, WON = 1, LOST = 2 }
T.DIFF   = { EASY = 0, HARD = 1 }
T.FB     = { NONE = 0, ABSENT = 1, PRESENT = 2, CORRECT = 3 }

-- keyboard layouts, as in lang.c; \1 = enter key, \2 = delete key
T.LAYOUT = {
  [0] = { "AZERTYUIOP", "QSDFGHJKLM", "\1WXCVBN\2" },
  [1] = { "QWERTYUIOP", "ASDFGHJKL",  "\1ZXCVBNM\2" },
}
-- valid words per language (all in data/<lang>_valid.txt), used as wrong guesses
T.WORDS = {
  [0] = { "TERRE", "PORTE", "TABLE", "CHIEN", "ROUGE", "BLANC", "MONDE", "JAUNE", "VERRE", "PLAGE", "LIVRE", "ROUTE",
          "SUCRE", "POMME", "NOIRE", "ARBRE", "FLEUR", "PIANO", "TIGRE", "LUNDI", "MARDI", "JEUDI" },
  [1] = { "SLATE", "CRANE", "ABBEY", "ALLEY", "APPLE", "EERIE", "LEVEL", "HOUSE", "WATER", "MONEY", "NIGHT", "WORLD",
          "MUSIC", "HEART", "PHONE", "BLOOD", "CHILD", "TRUTH", "POWER", "LIGHT", "BLACK", "WHITE" },
}

-- timings (frames) taken from main.c
T.REVEAL_FRAMES = 5 * 6          -- reveal_row: 6 frames per cell
T.END_PAUSE     = 150            -- finish_game: wait_or_key(150)
T.MARATHON_PAUSE = 120           -- marathon_after_guess: wait_or_key(120)

-- --- logging -------------------------------------------------------------------
local logfile = io.open(CFG.out .. "/" .. CFG.scenario .. ".log", "w")
T.failures, T.checks = 0, 0

function T.log(s)
  logfile:write(s, "\n"); logfile:flush()
  console:log(s)
end

function T.check(cond, name)
  T.checks = T.checks + 1
  if cond then T.log("PASS " .. name) else T.failures = T.failures + 1; T.log("FAIL " .. name) end
  return cond
end

function T.check_eq(actual, expected, name)
  return T.check(actual == expected, string.format("%s (got %s, expected %s)", name, tostring(actual), tostring(expected)))
end

-- --- frames and input ------------------------------------------------------------
function T.wait(n) for _ = 1, n do coroutine.yield() end end

-- press a button: held `hold` frames (default 2), then released for 3 frames
-- so that two consecutive presses are seen as two edges by the game
function T.press(key, hold)
  emu:addKey(key); T.wait(hold or 2); emu:clearKey(key); T.wait(3)
end

function T.shot(name)
  emu:screenshot(CFG.out .. "/" .. CFG.scenario .. "_" .. name .. ".png")
end

-- --- reading the ROM's memory --------------------------------------------------
local function u8(a) return emu:read8(a) end
local function u16(a) return emu:read16(a) end
local function u32(a) return emu:read32(a) end
local S, O = CFG.sym, CFG.off

function T.screen() return u8(S.current_screen) end
function T.frames() return u32(S.frames) end
function T.menu_item() return u8(S.menu_item) end     -- ints: the low byte is enough
function T.sub_item() return u8(S.sub_item) end
function T.records_page() return u8(S.records_page) end
function T.menu_lang() return u8(S.menu_lang) end

function T.game()
  local g = S.game
  local t = ""
  for i = 0, 4 do t = t .. string.char(u8(g + O["game.target"] + i)) end
  local cur = ""
  for i = 0, u8(g + O["game.cur_len"]) - 1 do cur = cur .. string.char(u8(g + O["game.current"] + i)) end
  return {
    lang = u8(g + O["game.lang"]), mode = u8(g + O["game.mode"]), status = u8(g + O["game.status"]),
    target = t, n_guesses = u8(g + O["game.n_guesses"]), cur_len = u8(g + O["game.cur_len"]), current = cur,
    guess = function(row)               -- 0-based row, returns the word
      local w = ""
      for i = 0, 4 do w = w .. string.char(u8(g + O["game.guesses"] + row * 5 + i)) end
      return w
    end,
    feedback = function(row, col) return u8(g + O["game.feedback"] + row * 5 + col) end,
    key_state = function(ch) return u8(g + O["game.key_state"] + string.byte(ch) - 65) end,
  }
end

function T.marathon()
  local m = S.marathon
  return {
    difficulty = u8(m + O["marathon.difficulty"]), hp = u8(m + O["marathon.hp"]),
    hp_max = u8(m + O["marathon.hp_max"]), score = u16(m + O["marathon.score"]),
    new_record = u8(m + O["marathon.new_record"]) ~= 0,
  }
end

function T.kb() return { row = u8(S.kb + O["kb.row"]), col = u8(S.kb + O["kb.col"]) } end

function T.time_attack()
  local t = S.ta
  local rank = u8(t + O["ta.rank"])
  return {
    length_idx = u8(t + O["ta.length_idx"]), total = u8(t + O["ta.total"]), done = u8(t + O["ta.done"]),
    missed = u8(t + O["ta.missed"]), frames = u32(t + O["ta.frames"]), running = u8(t + O["ta.running"]) ~= 0,
    rank = rank >= 128 and rank - 256 or rank,
  }
end

local function record_at(addr)
  local ini = ""
  for i = 0, 2 do ini = ini .. string.char(u8(addr + O["record.initials"] + i)) end
  return { frames = u32(addr + O["record.frames"]), initials = ini, used = u8(addr + O["record.used"]) ~= 0 }
end

function T.save()
  local s = S.save
  local dist = {}
  for i = 0, 5 do dist[i + 1] = u16(s + O["save.dist"] + 2 * i) end
  return {
    lang = u8(s + O["save.lang"]), sound_on = u8(s + O["save.sound_on"]),
    marathon_diff = u8(s + O["save.marathon_diff"]),
    played = u16(s + O["save.played"]), won = u16(s + O["save.won"]), lost = u16(s + O["save.lost"]),
    streak = u16(s + O["save.streak"]), max_streak = u16(s + O["save.max_streak"]), dist = dist,
    marathon_best = { [0] = u16(s + O["save.marathon_best"]), [1] = u16(s + O["save.marathon_best"] + 2) },
    ta_length = u8(s + O["save.ta_length"]),
    initials = string.char(u8(s + O["save.initials"]), u8(s + O["save.initials"] + 1), u8(s + O["save.initials"] + 2)),
    -- board(len_idx)[rank + 1] -> { frames, initials, used }
    board = function(len_idx)
      local b = {}
      for i = 0, 2 do
        b[i + 1] = record_at(s + O["save.ta_board"] + (len_idx * 3 + i) * O["sizeof.TimeRecord"])
      end
      return b
    end,
  }
end

-- --- sound monitor: highest envelope volume seen since the last reset --------
T.snd = { sq1 = 0, sq2 = 0, noise = 0, master = 0 }
function T.snd_reset() T.snd.sq1, T.snd.sq2, T.snd.noise = 0, 0, 0 end
local function snd_poll()
  local v1, v2, v4 = u16(0x04000062) >> 12, u16(0x04000068) >> 12, u16(0x04000078) >> 12
  if v1 > T.snd.sq1 then T.snd.sq1 = v1 end
  if v2 > T.snd.sq2 then T.snd.sq2 = v2 end
  if v4 > T.snd.noise then T.snd.noise = v4 end
  T.snd.master = (u16(0x04000084) >> 7) & 1
end

-- --- navigation --------------------------------------------------------------------
-- From power-on to the menu, choosing `lang` on the language screen.
function T.boot(lang)
  T.wait(20)
  T.check_eq(T.screen(), T.SCREEN.LANG, "boot shows the language screen")
  for _ = 1, 3 do
    if T.menu_lang() == lang then break end
    T.press(K.DOWN)
  end
  T.press(K.A); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.TITLE, "title screen after choosing the language")
  T.press(K.START); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.MENU, "menu after START")
end

-- Move the menu cursor to `item` (see T.MENU) without activating it.
function T.menu_go(item)
  for _ = 1, T.MENU.COUNT do
    if T.menu_item() == item then return true end
    T.press(K.DOWN)
  end
  return T.menu_item() == item
end

function T.menu_start(item)
  T.menu_go(item)
  T.press(K.A); T.wait(3)
end

-- Move the cursor of a sub screen (options, mode select) to `item`.
function T.sub_go(item, count)
  for _ = 1, count do
    if T.sub_item() == item then return true end
    T.press(K.DOWN)
  end
  return T.sub_item() == item
end

-- Options screen: press RIGHT on `opt` until reader() returns `value`, back to the menu.
function T.option_set(opt, reader, value)
  if T.screen() ~= T.SCREEN.OPTIONS then T.menu_start(T.MENU.OPTIONS) end
  T.sub_go(opt, T.OPT.COUNT)
  for _ = 1, 4 do
    if reader() == value then break end
    T.press(K.RIGHT)
  end
  local ok = reader() == value
  T.press(K.B); T.wait(3)
  return ok
end

function T.set_language(lang) return T.option_set(T.OPT.LANGUAGE, function() return T.save().lang end, lang) end
function T.set_sound(on) return T.option_set(T.OPT.SOUND, function() return T.save().sound_on end, on) end

-- Menu -> mode select -> game
function T.start_marathon(diff)
  T.menu_start(T.MENU.MARATHON)
  T.sub_go(diff, 2)
  T.press(K.A); T.wait(3)
end

function T.start_time_attack(len_idx)
  T.menu_start(T.MENU.TIME_ATTACK)
  T.sub_go(len_idx, 3)
  T.press(K.A); T.wait(3)
end

-- --- in-game helpers ---------------------------------------------------------------
local function find_key(layout, ch)
  for r = 1, 3 do
    local c = string.find(layout[r], ch, 1, true)
    if c then return r - 1, c - 1 end
  end
  error("key not in layout: " .. ch)
end

-- Move the keyboard cursor onto `ch` (a letter, "\1" enter or "\2" delete).
function T.goto_key(ch)
  local layout = T.LAYOUT[T.game().lang]
  local tr, tc = find_key(layout, ch)
  for _ = 1, 20 do
    local c = T.kb()
    if c.row == tr and c.col == tc then return true end
    if c.row ~= tr then
      T.press((tr - c.row) % 3 == 1 and K.DOWN or K.UP)
    else
      local n = #layout[c.row + 1]
      T.press((tc - c.col) % n <= n / 2 and K.RIGHT or K.LEFT)
    end
  end
  error("could not reach key " .. ch)
end

function T.type_word(w)
  for i = 1, #w do T.goto_key(w:sub(i, i)); T.press(K.A) end
end

-- START, then the reveal animation
function T.submit() T.press(K.START); T.wait(T.REVEAL_FRAMES + 5) end

function T.target() return T.game().target end

-- a valid word of the current language that is not the target (nor in `used`)
function T.wrong_word(used)
  local t = T.target()
  for _, w in ipairs(T.WORDS[T.game().lang]) do
    if w ~= t and not (used and used[w]) then
      if used then used[w] = true end
      return w
    end
  end
  error("no wrong word left")
end

function T.solve() T.type_word(T.target()); T.submit() end

-- SELECT then A (quit) or B (stay)
function T.quit(confirm) T.press(K.SELECT); T.wait(2); T.press(confirm and K.A or K.B); T.wait(3) end

-- --- scheduler --------------------------------------------------------------------
function T.run(scenario)
  local co = coroutine.create(function()
    scenario()
    T.log(string.format("END checks=%d failures=%d", T.checks, T.failures))
  end)
  callbacks:add("frame", function()
    snd_poll()
    if coroutine.status(co) ~= "dead" then
      local ok, err = coroutine.resume(co)
      if not ok then
        T.log("ERROR " .. tostring(err))
        emu:screenshot(CFG.out .. "/" .. CFG.scenario .. "_error.png")
        logfile:close()
        os.exit(1)
      end
    else
      logfile:close()
      os.exit(T.failures == 0 and 0 or 1)
    end
  end)
end
