-- @timeout 240
-- Marathon, easy: three missed words end the run with a score of 0 and do
-- not lower the saved high score.
T.run(function()
  T.boot(T.LANG.FR)
  local best = T.save().marathon_best[0]
  T.menu_set(T.MENU.MARATHON, function() return T.save().marathon_diff end, T.DIFF.EASY)
  T.press(T.K.A); T.wait(3)
  for life = 3, 1, -1 do
    T.check_eq(T.marathon().hp, life, "lives before the word: " .. life)
    local used = {}
    for _ = 1, 6 do T.type_word(T.wrong_word(used)); T.submit() end
    T.check_eq(T.game().status, T.STATUS.LOST, "word missed with " .. life .. " lives")
    T.press(T.K.A); T.wait(3)
  end
  T.check_eq(T.marathon().hp, 0, "no life left")
  T.check_eq(T.screen(), T.SCREEN.MARATHON_RESULT, "game over screen")
  T.check_eq(T.marathon().score, 0, "score 0")
  T.check(not T.marathon().new_record, "not a record")
  T.check_eq(T.save().marathon_best[0], best, "high score unchanged")
  T.shot("game_over")
  T.press(T.K.B); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.MENU, "B returns to the menu")
end)
