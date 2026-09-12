-- A classic game lost after six wrong guesses: the streak resets, the
-- result screen shows the word, B returns to the menu.
T.run(function()
  T.boot(T.LANG.FR)
  local before = T.save()
  T.menu_start(T.MENU.CLASSIC)
  local used = {}
  for i = 1, 6 do
    T.type_word(T.wrong_word(used)); T.submit()
    T.check_eq(T.game().n_guesses, i, "guess " .. i .. " accepted")
  end
  local g = T.game()
  T.check_eq(g.status, T.STATUS.LOST, "lost after six guesses")
  T.check_eq(g.cur_len, 0, "no more typing")
  T.shot("lost")
  T.press(T.K.A); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.RESULT, "result screen")
  T.shot("result")
  local s = T.save()
  T.check_eq(s.played, before.played + 1, "games played +1")
  T.check_eq(s.lost, before.lost + 1, "losses +1")
  T.check_eq(s.streak, 0, "streak reset")
  T.check_eq(s.max_streak, before.max_streak, "best streak unchanged")
  T.press(T.K.B); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.MENU, "B returns to the menu")
end)
