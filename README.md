# MotMot Advance

*Version française : [README.fr.md](README.fr.md)*

A word-guessing game for the Game Boy Advance in the spirit of the TV show
*Motus*: find a 5-letter word in 6 tries with green / yellow / grey feedback
on every letter, a D-pad driven virtual keyboard, two languages (French and
English), chiptune sound effects, statistics saved to SRAM, a Marathon
mode with lives and high scores, and a Time Attack mode with leaderboards. Written in C with libtonc — no assembly, no C++.

| Title | Menu | Game | Marathon | Time Attack |
|---|---|---|---|---|
| ![](docs/title.png) | ![](docs/menu.png) | ![](docs/game.png) | ![](docs/marathon.png) | ![](docs/time_attack.png) |

## Features

- Language selection at boot, title screen, main menu (Classic, Marathon,
  Time Attack, Records, Statistics, Options).
- **Classic mode**: a random word among ~500 common words, never repeating
  the last 8 words played.
- **Marathon mode**: random words one after another, with lives and a saved
  high score per difficulty. Easy: 3 lives, a missed word costs one. Hard:
  5 lives, every guess from the third one costs one — solve fast or bleed.
- **Time Attack**: 5, 10 or 15 words as fast as possible; the clock runs
  through the reveals, a missed word costs 30 s. Each length has its own
  top-3 leaderboard with arcade-style initials, shown on the Records screen
  and next to each length when starting a run.
- Guesses are validated against a large list (~8,700 English words,
  ~6,600 French words; accents are stripped, as in the TV show).
- AZERTY layout in French, QWERTY in English, with Enter / Backspace keys.
- Sound effects on the Game Boy tone generators: key clicks, buzzer on an
  unknown word, a different note for each revealed colour, win fanfare, loss
  jingle. Can be switched off in the menu (saved).
- Persistent statistics (SRAM): games played, wins, current streak, best
  streak, guess distribution, Marathon high scores, Time Attack leaderboards.
- The whole interface is translated into the selected language.

## Controls

| Button | Action |
|---|---|
| D-pad | Move the cursor on the virtual keyboard / navigate menus |
| A | Type the selected letter (or activate Enter / Backspace on the keyboard) |
| B | Delete the last letter |
| START | Submit the word |
| SELECT | Quit the game (asks for confirmation; the Time Attack clock stops meanwhile) |
| Left / Right | Change an option, turn the Records pages, move between initials |

## Building

Requirements: [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the
`gba-dev` group (devkitARM, libtonc, gbafix), GNU make, Python 3 with
[Pillow](https://pypi.org/project/pillow/) (tile generation).

```sh
make            # -> build/motmot.gba
make run        # launch the ROM in mGBA (override the path with MGBA=...)
make test       # unit tests on the host (tests/unit)
make emutest    # scenarios played in mGBA (tests/emu)
make check      # both
make clean
```

On Windows, run `make` from the MSYS2 shell shipped with devkitPro
(`DEVKITPRO=/opt/devkitpro`). If Python is not on that shell's PATH:
`make PYTHON="py -3"`.

The ROM declares an SRAM save (`SRAM_V113`); mGBA and flash-cart loaders
detect it automatically.

## Home-made toolchain (no grit)

Everything generated is produced at build time in `build/gen/`:

| Script | Purpose |
|---|---|
| `tools/make_assets.py` | Draws the source images `assets/*.png` (indexed PNGs) from ASCII pixel art: 6×7 font (A-Z, 0-9, punctuation, Enter/Backspace icons, bar segment), 16×16 grid cells and rounded keyboard keys with the letter pre-composed, cursor sprite, title background pattern. `make assets` regenerates them. |
| `tools/png2gba.py` | Converts a PNG into 4bpp tiles + BGR555 palette as C arrays. `--meta 2 2` (through `assets/<name>.opts`) emits tiles in 16×16 metatile order. |
| `tools/gen_wordlist.py` | Turns `data/<lang>_solutions.txt` and `data/<lang>_valid.txt` into C tables: solutions and sorted valid words (binary search). |
| `tools/build_wordlists.py` | Rebuilds the `data/*.txt` files from the external sources (needs network, `make wordlists`). |

Colours (green / yellow / grey / neutral) are not baked into the tiles: there
is one tile set per letter, coloured through palette banks (`SE_PALBANK`).

## Architecture

```
source/main.c        game loop and screens: language / title / menu / options / records / mode select / game / results / stats
source/logic.c       game rules (scoring with repeated letters, validation, typing) — no hardware dependency
source/lang.c        language table: word lists, keyboard layout, UI strings
source/render.c      Mode 0: BG0 text, BG1 grid + keyboard, BG2 title pattern, cursor sprite
source/sound.c       PSG sound effects: step sequencer on square 1, square 2 and noise
source/keyboard.c    cursor navigation over the virtual keyboard
source/input.c       key polling, D-pad auto-repeat
source/stats.c       SRAM read / write (statistics, RNG state, options)
source/rng.c         xorshift32
source/time_attack.c time formatting and leaderboards (pure, host-tested)
include/game_state.h state of one round (target, guesses, feedback, keyboard colours)
include/stats.h      saved data layout
```

Video memory: charblock 0 = font, charblock 1 = cells and keys,
charblock 2 = pattern; screenblocks 28/29/30; the only sprite is the cursor.

## Tests

See [tests/README.md](tests/README.md).

- `make test` — 101 unit tests on the host: the game modules (rules, keyboard,
  RNG, SRAM persistence, sound sequencer, Time Attack leaderboards, language
  tables and word lists) are
  compiled with `gcc` against a shim that replaces the GBA registers and SRAM
  with plain memory.
- `make emutest` — 14 scenarios played in mGBA by Lua scripts (boot, menu,
  keyboard, classic win/loss, quit confirmation, Marathon easy/hard/game over,
  Time Attack with initials and leaderboards, sound switch, language switch,
  SRAM persistence across a reboot, random
  input for 30000 frames). Assertions read the ROM's RAM; the emulator runs
  unthrottled so the whole suite takes about 15 s. Needs an mGBA 0.11
  development build (`--script`).
- `make check` — both.

## Word list sources

- English: [an-array-of-english-words](https://github.com/words/an-array-of-english-words)
  (MIT) as the dictionary; frequency ranking from
  [FrequencyWords](https://github.com/hermitdave/FrequencyWords)
  (OpenSubtitles, CC BY-SA 4.0); the
  [google-10000-english](https://github.com/first20hours/google-10000-english)
  list to keep only everyday words as solutions; a first-names list to drop
  proper nouns.
- French: [Lexique 3.83](http://www.lexique.org) (CC BY-SA 4.0) for forms,
  lemmas, part of speech and frequencies;
  [an-array-of-french-words](https://github.com/words/an-array-of-french-words)
  (MIT) to widen the accepted-word list.

Solutions are the 500 most frequent everyday words (French: 5-letter nouns,
adjectives, verbs and adverbs after accent stripping), minus a short block
list.

Code © 2026 Clément Perreau, MIT licence (see `LICENSE`). Published by Khopa.
