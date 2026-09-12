-- Scripted play session captured frame by frame for docs/demo.gif.
-- Run by tools/make_demo.py on top of tests/emu/lib.lua (same T.* helpers).
-- CFG.frames_dir receives one PNG every CFG.every frames; CFG.lang picks
-- the language (T.LANG index) on the boot screen.
local shot_n = 0
local capture = true
local function tick()
  if capture and T.frames() % CFG.every == 0 then
    shot_n = shot_n + 1
    emu:screenshot(string.format("%s/%05d.png", CFG.frames_dir, shot_n))
  end
end

T.run(function()
  -- capture on every frame the scenario yields
  local wait = T.wait
  T.wait = function(n) for _ = 1, n do coroutine.yield(); tick() end end

  T.wait(30)                                    -- language screen
  T.press(T.K.DOWN, 6); T.wait(20)              -- browse down...
  T.press(T.K.UP, 6); T.wait(20)                -- ...and back
  for _ = 1, CFG.lang do T.press(T.K.DOWN, 6); T.wait(12) end
  T.wait(20)
  T.press(T.K.A); T.wait(120)                   -- title, PRESS START blinking
  T.press(T.K.START); T.wait(40)                -- menu
  T.press(T.K.DOWN, 6); T.wait(20)
  T.press(T.K.DOWN, 6); T.wait(20)              -- TIME ATTACK highlighted
  T.press(T.K.UP, 6); T.wait(20)
  T.press(T.K.UP, 6); T.wait(30)                -- back on CLASSIQUE
  T.press(T.K.A); T.wait(40)                    -- classic game

  local target = T.target()
  local w = T.wrong_word()
  for i = 1, #w do T.goto_key(w:sub(i, i)); T.press(T.K.A, 4); T.wait(6) end
  T.wait(20)
  T.submit(); T.wait(50)                        -- reveal
  for i = 1, #target do T.goto_key(target:sub(i, i)); T.press(T.K.A, 4); T.wait(6) end
  T.wait(20)
  T.submit(); T.wait(110)                       -- win, fanfare, message
  T.press(T.K.A); T.wait(90)                    -- result screen
  T.press(T.K.B); T.wait(30)                    -- menu
  T.menu_go(T.MENU.MARATHON); T.wait(10)
  T.press(T.K.A); T.wait(60)                    -- difficulty select
  T.press(T.K.B); T.wait(10)
  T.menu_go(T.MENU.RECORDS); T.press(T.K.A); T.wait(60)
  T.press(T.K.RIGHT, 6); T.wait(40); T.press(T.K.RIGHT, 6); T.wait(40)
  capture = false
  T.log("frames captured: " .. shot_n)
end)
