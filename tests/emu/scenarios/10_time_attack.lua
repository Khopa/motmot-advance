-- @timeout 240
-- Time Attack, 5 words: the clock runs, there is no pause (SELECT abandons
-- the run at once), a missed word costs 30 s, finishing ranks on the
-- leaderboard, initials are entered arcade-style, the record shows up in the
-- records and on the mode select screen, a slower run does not replace it.
T.run(function()
  T.boot(T.LANG.FR)
  T.menu_start(T.MENU.TIME_ATTACK)
  T.check_eq(T.screen(), T.SCREEN.MODE_SELECT, "length selection first")
  T.shot("select")
  T.sub_go(0, 3)
  T.press(T.K.A); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.GAME, "time attack game screen")
  T.check_eq(T.save().ta_length, 0, "chosen length is saved")
  local t = T.time_attack()
  T.check_eq(T.game().mode, T.MODE.TIME_ATTACK, "mode is time attack")
  T.check_eq(t.total, 5, "five words to find")
  T.check_eq(t.done .. "/" .. t.missed, "0/0", "nothing done yet")
  T.check(t.running, "clock is running")
  local f1 = T.time_attack().frames
  T.wait(60)
  T.check_eq(T.time_attack().frames - f1, 60, "clock advances one unit per frame")
  T.shot("start")

  -- no pause against the clock: SELECT abandons the run immediately
  T.press(T.K.SELECT); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.MENU, "SELECT leaves a time attack at once (no question)")
  T.check(not T.time_attack().running, "clock stopped")
  T.check(not T.save().board(0)[1].used, "an abandoned run is not ranked")
  T.start_time_attack(0)
  T.check(T.time_attack().running, "new run, clock running")

  -- word 1 found: straight to the next word
  T.solve()
  T.check_eq(T.time_attack().done, 1, "word 1 done")
  T.wait(35)
  T.check_eq(T.game().n_guesses, 0, "word 2 loaded without a key press")
  T.check(T.time_attack().running, "clock still running")

  -- word 2 missed: +30 s, clock paused while the word is shown
  local used = {}
  for _ = 1, 6 do T.type_word(T.wrong_word(used)); T.submit() end
  local m = T.time_attack()
  T.check_eq(m.done .. "/" .. m.missed, "2/1", "word 2 counted as missed")
  T.check(not m.running, "clock paused on a missed word")
  local paused = m.frames
  T.wait(30)
  T.check_eq(T.time_attack().frames, paused, "no time elapses during the pause")
  T.shot("missed")
  T.press(T.K.A); T.wait(3)
  T.check(T.time_attack().running, "clock resumes with the next word")

  -- words 3-5
  for i = 3, 5 do
    T.solve()
    T.check_eq(T.time_attack().done, i, "word " .. i .. " done")
    if i < 5 then T.wait(35) end
  end
  T.wait(5)
  local r = T.time_attack()
  T.check(not r.running, "clock stopped at the end")
  T.check(r.frames >= T.TA_PENALTY, "penalty included in the time (" .. r.frames .. " frames)")
  T.check_eq(r.rank, 0, "first run is rank 1")
  T.check_eq(T.screen(), T.SCREEN.TA_RESULT, "time attack result screen")
  T.shot("result_initials")

  -- initials: UP twice on the first letter, A, DOWN once, A, A confirms the last
  T.press(T.K.UP); T.press(T.K.UP); T.press(T.K.A)
  T.press(T.K.DOWN); T.press(T.K.A)
  T.press(T.K.A); T.wait(40)
  local board = T.save().board(0)
  T.check(board[1].used, "record stored")
  T.check_eq(board[1].initials, "CZA", "initials entered: C (A+2), Z (A-1), A")
  T.check_eq(board[1].frames, r.frames, "record time is the run time")
  T.check_eq(T.save().initials, "CZA", "initials remembered for next time")
  T.check(not board[2].used, "only one record so far")
  T.shot("leaderboard")

  -- replay, then quit: an abandoned run leaves the board alone
  T.press(T.K.A); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.GAME, "A replays the same length")
  T.check_eq(T.time_attack().done, 0, "fresh run")
  T.press(T.K.SELECT); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.MENU, "SELECT returns to the menu")
  T.check_eq(T.save().board(0)[1].initials, "CZA", "board untouched by the abandoned run")

  -- the record shows on the records and mode select screens
  T.menu_start(T.MENU.RECORDS)
  T.shot("records")
  T.press(T.K.B); T.wait(3)
  T.menu_start(T.MENU.TIME_ATTACK)
  T.shot("select_with_record")
  T.press(T.K.B); T.wait(3)

  -- a slower 5-word run (one miss more) ranks second, initials default to the last ones
  T.start_time_attack(0)
  for i = 1, 5 do
    if i <= 2 then
      local u = {}
      for _ = 1, 6 do T.type_word(T.wrong_word(u)); T.submit() end
      T.press(T.K.A); T.wait(3)
    else
      T.solve(); T.wait(35)
    end
  end
  T.wait(5)
  T.check_eq(T.time_attack().rank, 1, "slower run ranks second")
  T.press(T.K.START); T.wait(40)                -- START confirms the default initials
  board = T.save().board(0)
  T.check_eq(board[1].initials, "CZA", "best record kept in first place")
  T.check_eq(board[2].initials, "CZA", "second place uses the remembered initials")
  T.check(board[2].frames > board[1].frames, "board sorted by time")
  T.press(T.K.B); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.MENU, "B returns to the menu")
end)
