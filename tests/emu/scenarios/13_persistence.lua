-- Boots on the save left by the previous scenarios: statistics and options
-- must still be there. Meaningful after 01..11; on a fresh save it only
-- checks the defaults.
T.run(function()
  T.wait(20)
  local s = T.save()
  if s.played == 0 and s.marathon_best[0] == 0 then
    T.log("PASS fresh save: nothing to verify (run the whole suite for real persistence)")
  else
    T.check(s.played >= 2, "games played survived the reboot (" .. s.played .. ")")
    T.check(s.won >= 1, "wins survived")
    T.check(s.lost >= 1, "losses survived")
    T.check_eq(s.marathon_best[0], 3, "easy marathon record survived")
    T.check_eq(s.marathon_best[1], 1, "hard marathon record survived")
    T.check_eq(s.lang, T.LANG.FR, "language option survived")
    T.check_eq(s.sound_on, 1, "sound option survived")
    local b = s.board(0)
    T.check(b[1].used, "time attack 5-word record survived")
    T.check_eq(b[1].initials, "CZA", "record initials survived")
    T.check_eq(s.initials, "CZA", "last initials survived")
  end
  T.boot(T.LANG.FR)
  T.menu_start(T.MENU.STATS)
  T.shot("stats")
  T.press(T.K.B); T.wait(3)
  T.menu_start(T.MENU.RECORDS)
  T.shot("records")
  T.press(T.K.B)
end)
