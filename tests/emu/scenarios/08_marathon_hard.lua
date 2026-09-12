-- Marathon, hard: 5 lives, every guess from the third one costs a life,
-- the run ends when the lives are gone (game over screen, record saved).
T.run(function()
  T.boot(T.LANG.FR)
  T.menu_set(T.MENU.MARATHON, function() return T.save().marathon_diff end, T.DIFF.HARD)
  T.press(T.K.A); T.wait(3)
  local m = T.marathon()
  T.check_eq(m.difficulty, T.DIFF.HARD, "hard difficulty")
  T.check_eq(m.hp .. "/" .. m.hp_max, "5/5", "five lives")

  local used = {}
  T.type_word(T.wrong_word(used)); T.submit()
  T.check_eq(T.marathon().hp, 5, "guess 1: no life lost")
  T.type_word(T.wrong_word(used)); T.submit()
  T.check_eq(T.marathon().hp, 5, "guess 2: no life lost")
  T.snd_reset()
  T.type_word(T.wrong_word(used)); T.submit(); T.wait(20)
  T.check_eq(T.marathon().hp, 4, "guess 3 costs a life")
  T.check(T.snd.sq1 >= 11, "hurt sound played")
  T.shot("life_lost")
  T.type_word(T.wrong_word(used)); T.submit(); T.wait(20)
  T.check_eq(T.marathon().hp, 3, "guess 4 costs a life")
  T.solve(); T.wait(20)
  T.check_eq(T.marathon().hp, 2, "the winning 5th guess costs a life too")
  T.check_eq(T.marathon().score, 1, "but the word counts")
  T.press(T.K.A); T.wait(3)
  T.check_eq(T.game().n_guesses, 0, "next word")

  -- drain the last two lives: game over even though the word is not finished
  used = {}
  for i = 1, 4 do T.type_word(T.wrong_word(used)); T.submit(); T.wait(25) end
  T.check_eq(T.marathon().hp, 0, "no life left after the 4th guess")
  T.wait(T.MARATHON_PAUSE + 10)
  T.check_eq(T.screen(), T.SCREEN.MARATHON_RESULT, "game over screen")
  T.check_eq(T.marathon().score, 1, "final score 1")
  T.check(T.marathon().new_record, "first hard run is a record")
  T.check_eq(T.save().marathon_best[1], 1, "hard high score saved")
  T.check_eq(T.save().marathon_best[0], 3, "easy high score untouched")
  T.shot("game_over")
  T.press(T.K.B); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.MENU, "B returns to the menu")
end)
