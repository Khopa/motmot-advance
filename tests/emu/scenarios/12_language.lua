-- Switching languages: QWERTY / QWERTZ layouts, each language accepts its
-- own words and rejects a French one, and back to French/AZERTY.
T.run(function()
  T.boot(T.LANG.FR)
  T.set_language(T.LANG.EN)
  T.shot("menu_en")
  T.menu_start(T.MENU.CLASSIC)
  T.check_eq(T.game().lang, T.LANG.EN, "game in English")
  T.check_eq(T.kb().row .. "," .. T.kb().col, "0,0", "cursor on the first key")
  T.press(T.K.A)
  T.check_eq(T.game().current, "Q", "first key of the top row is Q (QWERTY)")
  T.press(T.K.B)
  T.type_word("TERRE"); T.press(T.K.START); T.wait(3)
  T.check_eq(T.game().n_guesses, 0, "a French word is not in the English list")
  for _ = 1, 5 do T.press(T.K.B) end
  T.type_word("CRANE"); T.submit()
  T.check_eq(T.game().n_guesses, 1, "an English word is accepted")
  T.shot("game_en")
  T.quit(true)

  -- Spanish, German, Italian: language applied, own word list, layout
  local others = {
    { T.LANG.ES, "es", "MUNDO", "Y" },
    { T.LANG.DE, "de", "NICHT", "Z" },     -- QWERTZ: Z is the 6th key of the top row
    { T.LANG.IT, "it", "TEMPO", "Y" },
  }
  for _, e in ipairs(others) do
    T.set_language(e[1])
    T.check_eq(T.save().lang, e[1], e[2] .. ": language saved")
    T.shot("menu_" .. e[2])
    T.menu_start(T.MENU.CLASSIC)
    T.check_eq(T.game().lang, e[1], e[2] .. ": game language")
    for _ = 1, 5 do T.press(T.K.RIGHT) end          -- 6th key of the top row
    T.press(T.K.A)
    T.check_eq(T.game().current, e[4], e[2] .. ": 6th key of the top row is " .. e[4])
    T.press(T.K.B)
    T.type_word("JAUNE"); T.press(T.K.START); T.wait(3)
    T.check_eq(T.game().n_guesses, 0, e[2] .. ": a French word is rejected")
    for _ = 1, 5 do T.press(T.K.B) end
    T.type_word(e[3]); T.submit()
    T.check_eq(T.game().n_guesses, 1, e[2] .. ": " .. e[3] .. " is accepted")
    T.shot("game_" .. e[2])
    T.quit(true)
  end

  T.set_language(T.LANG.FR)
  T.menu_start(T.MENU.CLASSIC)
  T.check_eq(T.game().lang, T.LANG.FR, "game in French again")
  T.press(T.K.A)
  T.check_eq(T.game().current, "A", "first key of the top row is A (AZERTY)")
  T.quit(true)
end)
