-- Main menu navigation and wrap-around, every entry opens its screen and B
-- comes back, options (language, sound) are saved, B on the menu returns to
-- the title.
T.run(function()
  T.boot(T.LANG.FR)
  T.check_eq(T.menu_item(), T.MENU.CLASSIC, "cursor starts on CLASSIC")
  T.shot("menu")

  -- DOWN cycles through every item and wraps
  local seen = {}
  for _ = 1, T.MENU.COUNT do seen[T.menu_item()] = true; T.press(T.K.DOWN) end
  local all = true
  for i = 0, T.MENU.COUNT - 1 do if not seen[i] then all = false end end
  T.check(all, "DOWN visits every menu item")
  T.check_eq(T.menu_item(), T.MENU.CLASSIC, "DOWN x" .. T.MENU.COUNT .. " wraps around")
  T.press(T.K.UP)
  T.check_eq(T.menu_item(), T.MENU.OPTIONS, "UP from the first item wraps to the last")
  -- holding DOWN auto-repeats
  emu:addKey(T.K.DOWN); T.wait(30); emu:clearKey(T.K.DOWN); T.wait(3)
  T.check(T.menu_item() ~= T.MENU.OPTIONS and T.menu_item() ~= T.MENU.CLASSIC, "holding DOWN auto-repeats")

  -- every entry opens its screen, B comes back with the cursor kept
  local screens = {
    { T.MENU.MARATHON, T.SCREEN.MODE_SELECT, "marathon" },
    { T.MENU.TIME_ATTACK, T.SCREEN.MODE_SELECT, "time_attack" },
    { T.MENU.RECORDS, T.SCREEN.RECORDS, "records" },
    { T.MENU.STATS, T.SCREEN.STATS, "stats" },
    { T.MENU.OPTIONS, T.SCREEN.OPTIONS, "options" },
  }
  for _, e in ipairs(screens) do
    T.menu_start(e[1])
    T.check_eq(T.screen(), e[2], e[3] .. " entry opens its screen")
    T.shot(e[3])
    T.press(T.K.B); T.wait(3)
    T.check_eq(T.screen(), T.SCREEN.MENU, "B leaves " .. e[3])
    T.check_eq(T.menu_item(), e[1], "cursor stays on " .. e[3])
  end

  -- records pages
  T.menu_start(T.MENU.RECORDS)
  T.check_eq(T.records_page(), 0, "records open on the first page")
  T.press(T.K.RIGHT); T.check_eq(T.records_page(), 1, "RIGHT: next page")
  T.press(T.K.LEFT); T.press(T.K.LEFT)
  T.check_eq(T.records_page(), 3, "LEFT wraps to the marathon page")
  T.shot("records_marathon")
  T.press(T.K.RIGHT); T.check_eq(T.records_page(), 0, "RIGHT wraps back to the first page")
  T.press(T.K.B); T.wait(3)

  -- options: language and sound, saved to SRAM
  T.menu_start(T.MENU.OPTIONS)
  T.check_eq(T.sub_item(), T.OPT.LANGUAGE, "options cursor starts on LANGUAGE")
  T.press(T.K.RIGHT)
  T.check_eq(T.save().lang, T.LANG.EN, "RIGHT on LANGUAGE switches to English (saved)")
  T.shot("options_en")
  T.press(T.K.A)
  T.check_eq(T.save().lang, T.LANG.ES, "A on LANGUAGE moves to the next one (Spanish)")
  T.press(T.K.LEFT)
  T.check_eq(T.save().lang, T.LANG.EN, "LEFT goes back to English")
  T.press(T.K.LEFT)
  T.check_eq(T.save().lang, T.LANG.FR, "LEFT again: French")
  T.press(T.K.LEFT)
  T.check_eq(T.save().lang, T.LANG.IT, "LEFT wraps to Italian")
  T.press(T.K.RIGHT)
  T.check_eq(T.save().lang, T.LANG.FR, "RIGHT wraps back to French")
  T.press(T.K.DOWN)
  T.check_eq(T.sub_item(), T.OPT.SOUND, "DOWN moves to SOUND")
  T.press(T.K.RIGHT)
  T.check_eq(T.save().sound_on, 0, "RIGHT on SOUND turns it off (saved)")
  T.shot("options_sound_off")
  T.press(T.K.A)
  T.check_eq(T.save().sound_on, 1, "A on SOUND turns it on again")
  T.press(T.K.UP)
  T.check_eq(T.sub_item(), T.OPT.LANGUAGE, "UP wraps back to LANGUAGE")
  T.press(T.K.B); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.MENU, "B leaves the options")

  -- mode select cursors remember the saved choice
  T.menu_start(T.MENU.MARATHON)
  T.check_eq(T.sub_item(), T.save().marathon_diff, "marathon select starts on the saved difficulty")
  T.press(T.K.DOWN); T.press(T.K.DOWN)
  T.check_eq(T.sub_item(), T.save().marathon_diff, "two DOWN wrap around the two difficulties")
  T.press(T.K.B); T.wait(3)
  T.menu_start(T.MENU.TIME_ATTACK)
  T.check_eq(T.sub_item(), T.save().ta_length, "time attack select starts on the saved length")
  T.press(T.K.UP)
  T.check_eq(T.sub_item(), 2, "UP from the first length wraps to 15 words")
  T.press(T.K.B); T.wait(3)

  -- title and back
  T.press(T.K.B); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.TITLE, "B on the menu returns to the title")
  T.press(T.K.START); T.wait(3)
  T.check_eq(T.screen(), T.SCREEN.MENU, "START returns to the menu, language not asked again")
end)
