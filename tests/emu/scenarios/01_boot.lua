-- @fresh
-- Power-on with a blank save: language screen, title, menu, default options.
T.run(function()
  T.wait(20)
  T.check_eq(T.screen(), T.SCREEN.LANG, "language screen comes first")
  T.check_eq(T.menu_lang(), T.LANG.FR, "French is preselected on a blank save")
  T.shot("language")

  -- any direction toggles between the two languages
  T.press(T.K.DOWN); T.check_eq(T.menu_lang(), T.LANG.EN, "DOWN selects English")
  T.press(T.K.UP);   T.check_eq(T.menu_lang(), T.LANG.FR, "UP selects French again")
  T.press(T.K.RIGHT); T.check_eq(T.menu_lang(), T.LANG.EN, "RIGHT toggles too")
  T.press(T.K.LEFT);  T.check_eq(T.menu_lang(), T.LANG.FR, "LEFT toggles back")

  T.press(T.K.A); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.TITLE, "A confirms and shows the title")
  T.check_eq(T.save().lang, T.LANG.FR, "language saved to SRAM")
  T.shot("title")

  T.wait(40)
  T.check_eq(T.screen(), T.SCREEN.TITLE, "title waits for START")
  T.press(T.K.START); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.MENU, "START opens the menu")
  T.check_eq(T.menu_item(), T.MENU.CLASSIC, "CLASSIC is highlighted by default")
  T.shot("menu")

  local s = T.save()
  T.check_eq(s.played, 0, "no game played yet")
  T.check_eq(s.sound_on, 1, "sound on by default")
  T.check_eq(s.marathon_diff, T.DIFF.EASY, "marathon defaults to easy")
  T.check_eq(s.marathon_best[0] + s.marathon_best[1], 0, "no marathon record yet")
  T.check_eq(T.snd.master, 1, "sound hardware enabled")
end)
