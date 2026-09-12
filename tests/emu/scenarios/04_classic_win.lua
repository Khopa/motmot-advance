-- A classic game won on the second guess: invalid words are refused with a
-- buzzer, a valid guess is revealed, the win updates the statistics, the
-- result screen leads to a new game or back to the menu.
T.run(function()
  T.boot(T.LANG.FR)
  local before = T.save()
  T.menu_start(T.MENU.CLASSIC)
  local target = T.target()
  T.log("target = " .. target)

  -- too short
  T.type_word("AB")
  T.snd_reset()
  T.press(T.K.START); T.wait(3)
  T.check_eq(T.game().n_guesses, 0, "two letters are not a guess")
  T.check_eq(T.game().cur_len, 2, "the letters stay in the row")
  T.check(T.snd.sq1 > 0 and T.snd.noise > 0, "buzzer on a too-short word")
  T.shot("too_short")
  T.press(T.K.B); T.press(T.K.B)

  -- unknown word
  T.snd_reset()
  T.type_word("AAAAA")
  T.check(T.snd.sq1 > 0 and T.snd.noise == 0, "key clicks while typing (no noise)")
  T.snd_reset()
  T.press(T.K.START); T.wait(3)
  T.check_eq(T.game().n_guesses, 0, "AAAAA is not in the word list")
  T.check(T.snd.noise > 0, "buzzer on an unknown word")
  T.shot("unknown_word")
  for _ = 1, 5 do T.press(T.K.B) end

  -- a valid wrong word
  local w = T.wrong_word()
  T.type_word(w); T.submit()
  local g = T.game()
  T.check_eq(g.n_guesses, 1, "valid word accepted")
  T.check_eq(g.status, T.STATUS.PLAYING, "game goes on")
  T.check_eq(g.guess(0), w, "guess stored in row 0")
  -- feedback consistency: green exactly where letters match the target
  local ok = true
  for i = 0, 4 do
    local exact = w:sub(i + 1, i + 1) == target:sub(i + 1, i + 1)
    if (g.feedback(0, i) == T.FB.CORRECT) ~= exact then ok = false end
    if g.feedback(0, i) == T.FB.NONE then ok = false end
  end
  T.check(ok, "feedback colours are consistent with the target")
  T.check(g.key_state(w:sub(1, 1)) ~= T.FB.NONE, "keyboard remembers the colour of a used letter")
  T.shot("after_wrong_guess")

  -- win
  T.snd_reset()
  T.solve()
  g = T.game()
  T.check_eq(g.status, T.STATUS.WON, "target found")
  T.check_eq(g.n_guesses, 2, "won in two")
  T.check(T.snd.sq1 >= 12, "win fanfare played")
  T.shot("won")
  T.press(T.K.A); T.wait(3)                    -- skip the pause
  T.check_eq(T.screen(), T.SCREEN.RESULT, "result screen")
  T.shot("result")

  local s = T.save()
  T.check_eq(s.played, before.played + 1, "games played +1")
  T.check_eq(s.won, before.won + 1, "wins +1")
  T.check_eq(s.streak, before.streak + 1, "streak +1")
  T.check(s.max_streak >= s.streak, "best streak >= streak")
  T.check_eq(s.dist[2], before.dist[2] + 1, "distribution: one more win in 2")

  -- A plays again in the same mode, B goes to the menu
  T.press(T.K.A); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.GAME, "A on the result screen starts a new game")
  T.check_eq(T.game().n_guesses, 0, "fresh grid")
  T.check(T.game().target ~= target, "a different word (no immediate repeat)")
  T.quit(true)
  T.check_eq(T.screen(), T.SCREEN.MENU, "back to the menu")
end)
