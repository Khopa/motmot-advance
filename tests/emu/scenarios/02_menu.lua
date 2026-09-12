-- Menu navigation, wrap-around, options (language, sound, difficulty) and
-- their persistence in SRAM, back to the title with B.
T.run(function()
  T.boot(T.LANG.FR)
  T.check_eq(T.menu_item(), T.MENU.CLASSIC, "cursor starts on CLASSIC")

  -- DOWN cycles through every item and wraps
  local seen = {}
  for _ = 1, T.MENU.COUNT do seen[T.menu_item()] = true; T.press(T.K.DOWN) end
  local all = true
  for i = 0, T.MENU.COUNT - 1 do if not seen[i] then all = false end end
  T.check(all, "DOWN visits every menu item")
  T.check_eq(T.menu_item(), T.MENU.CLASSIC, "DOWN x" .. T.MENU.COUNT .. " wraps around")
  T.press(T.K.UP)
  T.check_eq(T.menu_item(), T.MENU.LANG, "UP from CLASSIC goes to LANGUAGE")
  T.press(T.K.UP)
  T.check_eq(T.menu_item(), T.MENU.SOUND, "UP from the first item wraps to the last")

  -- language option
  T.menu_go(T.MENU.LANG)
  T.press(T.K.RIGHT)
  T.check_eq(T.save().lang, T.LANG.EN, "RIGHT on LANGUAGE switches to English (saved)")
  T.shot("menu_en")
  T.press(T.K.A)
  T.check_eq(T.save().lang, T.LANG.FR, "A on LANGUAGE toggles it back")
  T.press(T.K.LEFT)
  T.check_eq(T.save().lang, T.LANG.EN, "LEFT toggles as well")
  T.press(T.K.LEFT)
  T.check_eq(T.save().lang, T.LANG.FR, "back to French")

  -- sound option
  T.menu_go(T.MENU.SOUND)
  T.press(T.K.RIGHT)
  T.check_eq(T.save().sound_on, 0, "RIGHT on SOUND turns it off (saved)")
  T.shot("menu_sound_off")
  T.press(T.K.A)
  T.check_eq(T.save().sound_on, 1, "A on SOUND turns it on again")

  -- marathon difficulty: LEFT/RIGHT change it, A starts (checked in 07)
  T.menu_go(T.MENU.MARATHON)
  T.press(T.K.RIGHT)
  T.check_eq(T.save().marathon_diff, T.DIFF.HARD, "RIGHT on MARATHON selects hard (saved)")
  T.press(T.K.RIGHT)
  T.check_eq(T.save().marathon_diff, T.DIFF.EASY, "RIGHT again wraps to easy")
  T.press(T.K.LEFT)
  T.check_eq(T.save().marathon_diff, T.DIFF.HARD, "LEFT selects hard")
  T.press(T.K.LEFT)
  T.check_eq(T.save().marathon_diff, T.DIFF.EASY, "LEFT again: easy")

  -- statistics screen and back
  T.menu_start(T.MENU.STATS)
  T.check_eq(T.screen(), T.SCREEN.STATS, "STATISTICS opens the statistics screen")
  T.shot("stats_empty")
  T.press(T.K.B); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.MENU, "B leaves the statistics")
  T.check_eq(T.menu_item(), T.MENU.STATS, "cursor stays on STATISTICS")

  -- title and back
  T.press(T.K.B); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.TITLE, "B on the menu returns to the title")
  T.press(T.K.START); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.MENU, "START returns to the menu, language not asked again")
end)
