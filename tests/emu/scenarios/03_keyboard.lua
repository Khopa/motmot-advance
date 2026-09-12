-- Virtual keyboard in a classic game: cursor wrap-around, row changes,
-- auto-repeat, typing, the 6th letter is refused, delete key, enter key.
T.run(function()
  T.boot(T.LANG.FR)
  T.menu_start(T.MENU.CLASSIC)
  T.check_eq(T.screen(), T.SCREEN.GAME, "classic game screen")
  local g = T.game()
  T.check_eq(g.mode, T.MODE.CLASSIC, "mode is classic")
  T.check_eq(g.lang, T.LANG.FR, "game language is French")
  T.check_eq(#g.target, 5, "a target word is set")
  T.check_eq(T.kb().row .. "," .. T.kb().col, "0,0", "cursor starts on the first key")

  T.press(T.K.LEFT)
  T.check_eq(T.kb().col, 9, "LEFT from the first key wraps to the last")
  T.press(T.K.RIGHT)
  T.check_eq(T.kb().col, 0, "RIGHT wraps back")
  T.press(T.K.UP)
  T.check_eq(T.kb().row, 2, "UP from the top row wraps to the bottom row")
  T.press(T.K.DOWN)
  T.check_eq(T.kb().row, 0, "DOWN wraps back to the top")

  -- hold RIGHT: auto-repeat moves several keys
  emu:addKey(T.K.RIGHT); T.wait(40); emu:clearKey(T.K.RIGHT); T.wait(3)
  T.check(T.kb().col >= 3, "holding RIGHT auto-repeats (col " .. T.kb().col .. ")")

  -- type letters with A
  T.type_word("AZERT")
  T.check_eq(T.game().current, "AZERT", "five letters typed")
  T.goto_key("Y"); T.press(T.K.A)
  T.check_eq(T.game().cur_len, 5, "sixth letter is refused")
  T.shot("row_full")

  T.press(T.K.B)
  T.check_eq(T.game().current, "AZER", "B deletes the last letter")
  T.goto_key("\2"); T.press(T.K.A)
  T.check_eq(T.game().current, "AZE", "the delete key on the keyboard deletes too")
  for _ = 1, 5 do T.press(T.K.B) end
  T.check_eq(T.game().cur_len, 0, "B on an empty row does nothing")

  -- enter key on the keyboard behaves like START
  T.goto_key("\1"); T.press(T.K.A); T.wait(3)
  T.check_eq(T.game().n_guesses, 0, "enter on an empty row: nothing submitted")
  T.type_word("TERRE")
  T.goto_key("\1"); T.press(T.K.A); T.wait(T.REVEAL_FRAMES + 5)
  T.check_eq(T.game().n_guesses, 1, "enter key submits a full valid row")
  T.check_eq(T.game().guess(0), "TERRE", "the guess is stored")
  T.shot("after_enter_key")
  T.quit(true)
end)
