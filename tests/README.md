# Tests

Two layers, both runnable with one command and both exit non-zero on failure:

| Command | What runs | Time |
|---|---|---|
| `make test` | host unit tests (`tests/unit`) — the game modules compiled on the PC | < 1 s |
| `make emutest` | scenarios played in mGBA (`tests/emu`) — the real ROM, driven by Lua | ~15 s |
| `make check` | both | |

Both layers need a build first (`make`); `make check` takes care of it.

## Unit tests (`tests/unit`)

`logic.c`, `keyboard.c`, `rng.c`, `stats.c`, `sound.c`, `time_attack.c` and
`lang.c` (with the generated word lists) are compiled with the host `gcc` and `-DHOST_TEST`.
In that mode `include/common.h` pulls in `tests/unit/host_shim.h` instead of
libtonc: I/O registers become slots of a `host_io[]` array and cartridge SRAM
becomes `host_sram[]`, so a test can check what the code *would* have written
to the hardware (`REG_SND1CNT`, `REG_WAITCNT`, the SRAM image...).

Framework: `tests/unit/test.h` — a test is a `TEST(name)` function using
`CHECK(cond)`, `CHECK_EQ(actual, expected)` or `CHECK_MEM(a, b, n)`; a suite
lists its tests in a `TestCase` array and `SUITE(name)` generates the runner;
`main.c` calls every suite. Each test starts from a clean shim (`host_reset()`).

```
build/unit_tests            # run all
build/unit_tests -v         # print every test name
build/unit_tests -f sound   # only tests whose suite or name contains "sound"
make test TESTFLAGS="-f stats -v"
```

| Suite | Covers |
|---|---|
| `logic` | scoring incl. repeated letters and a never-overclaim property, validation (binary search edges), typing/backspace, submit results, win/loss, keyboard colour upgrades |
| `keyboard` | row lengths, screen centring, horizontal/vertical wrap, column mapping between rows of different lengths |
| `rng` | determinism, seed 0, save/restore, range bounds and coverage, no short cycle |
| `stats` | defaults on blank SRAM, wait states, byte-for-byte save image, round trip, checksum/magic/version/range corruption, streaks and distribution, recent-words ring |
| `sound` | init registers, every effect's channel/volume/rate/timing, rests, replacement, termination, the off switch |
| `lang` | word lists sorted/unique/upper-case, solutions ⊂ valid, keyboard layouts complete, UI strings fit the screen and the font |
| `time_attack` | MM:SS.CC formatting and capping, leaderboard ranking / insertion / shifting / rejection |

To add a test: write `TEST(...)` in the relevant `test_*.c`, add `T(...)` to
the suite array. To add a suite: new file + `run_<suite>()` declared and called
in `main.c` (the Makefile picks up every `tests/unit/*.c`).

## Emulator scenarios (`tests/emu`)

`run.py` runs each `scenarios/*.lua` in mGBA (`--script`), on a private copy
of the ROM (`build/emu/rom.gba` + `rom.sav`) with video/audio sync disabled so
the emulator runs unthrottled (a full scenario takes about a second).
A scenario is a Lua function driven frame by frame by `lib.lua`:

```lua
-- @fresh                      -- optional: start from a blank save file
T.run(function()
  T.boot(T.LANG.FR)            -- power on, choose the language, reach the menu
  T.menu_start(T.MENU.CLASSIC)
  T.type_word("TERRE"); T.submit()
  T.check_eq(T.game().n_guesses, 1, "guess accepted")
  T.shot("after_guess")        -- tests/out/<scenario>_after_guess.png
end)
```

Assertions read the ROM's memory rather than pixels: symbol addresses come
from the ELF (`arm-none-eabi-nm`) and struct offsets from `offsets.c`,
compiled on the host at run time — the Lua side never hard-codes a layout.
Helpers (see `lib.lua` for the full list):

| Helper | Purpose |
|---|---|
| `T.press(key, hold)`, `T.wait(n)` | input with clean edges, frame waits |
| `T.boot(lang)`, `T.menu_go/menu_start`, `T.sub_go` | navigation using the menu cursors read from RAM |
| `T.start_marathon(diff)`, `T.start_time_attack(len)`, `T.set_language`, `T.set_sound` | mode select and options screens |
| `T.goto_key(ch)`, `T.type_word(w)`, `T.submit()`, `T.solve()`, `T.wrong_word(used)` | play through the virtual keyboard |
| `T.game()`, `T.marathon()`, `T.time_attack()`, `T.save()` (incl. `board(len)`), `T.kb()`, `T.screen()` | state snapshots |
| `T.snd`, `T.snd_reset()` | highest envelope volume seen on each PSG channel |
| `T.check`, `T.check_eq`, `T.log`, `T.shot` | results (PASS/FAIL lines in `tests/out/<scenario>.log`) |

```
python tests/emu/run.py                 # all scenarios, stop at the first failure
python tests/emu/run.py --keep-going    # run everything
python tests/emu/run.py marathon        # every scenario whose name contains "marathon"
python tests/emu/run.py 04 -v           # one scenario, print its log
python tests/emu/run.py --slow 07       # watch it at normal speed
python tests/emu/run.py --list
make emutest SCENARIO=06_quit
```

| Scenario | Covers |
|---|---|
| `01_boot` | language screen first, five languages with wrap-around, title, START, menu defaults, blank-save defaults |
| `02_menu` | cursor wrap and auto-repeat, every entry opens its screen and B returns, records pages, options saved, mode-select cursors, title and back |
| `03_keyboard` | cursor wrap, row changes, auto-repeat, 5-letter limit, B / delete key, enter key |
| `04_classic_win` | too short + unknown word (buzzer), a valid guess and its colours, win in two, stats update, result screen, replay |
| `05_classic_lose` | six guesses, LOST, streak reset, result screen |
| `06_quit` | SELECT confirmation: B and SELECT cancel, A and START confirm, D-pad blocked, stats untouched |
| `07_marathon_easy` | 3 lives, chained words, pause skip/auto-end, a miss costs a life, keyboard reset, quit records the score, replay resets, lower score keeps the record |
| `08_marathon_hard` | 5 lives, guesses 1-2 free, 3rd+ cost a life (hurt sound), winning on a costly guess, game over mid-word, records per difficulty |
| `09_marathon_gameover_easy` | three misses end the run with score 0 |
| `10_time_attack` | clock runs and stops on the quit question, missed word = +30 s with the clock paused, 5 words finished, rank, initials entry (UP/DOWN/A/START), leaderboard, replay/abandon, records and mode-select display, a slower run ranks second |
| `11_sound` | effects on: menu tick, clicks, reveal notes; off: silence everywhere; setting kept |
| `12_language` | English, Spanish, German (QWERTZ) and Italian: layout, own word list accepts / French word rejected; back to French/AZERTY |
| `13_persistence` | reboot on the save left by the previous scenarios (stats, options, marathon bests, time attack record and initials) |
| `14_monkey` | 30000 frames of random input: no freeze, PC always in ROM/IWRAM/BIOS |

Scenarios run in file-name order and share the save file unless they start
with `-- @fresh`; `-- @timeout N` overrides the 120 s limit. Requirements: an
mGBA build with `--script` (0.11 development builds, `--mgba` or `MGBA=` to
point at it), the devkitARM `nm`, and a host `gcc` (MSYS2's works from any
Windows shell).

Screenshots and logs land in `tests/out/` (git-ignored).
