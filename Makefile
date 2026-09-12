# MotMot Advance — Makefile
# Build from an MSYS2 shell with devkitPro installed (DEVKITPRO=/opt/devkitpro):
#     make            -> build/motmot.gba
#     make run        -> launch in mGBA
#     make test       -> unit tests on the host (tests/unit)
#     make emutest    -> scenarios in mGBA (tests/emu, needs a build with --script)
#     make check      -> both
#     make clean

TARGET   := motmot
BUILD    := build
GEN      := $(BUILD)/gen

DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
PREFIX   := $(DEVKITARM)/bin/arm-none-eabi-
CC       := $(PREFIX)gcc
OBJCOPY  := $(PREFIX)objcopy
GBAFIX   := $(DEVKITPRO)/tools/bin/gbafix
MGBA     ?= /c/Program\ Files/mGBA/mGBA.exe
PYTHON   ?= python
HOSTCC   ?= gcc

GAME_TITLE := MOTMOTADV
GAME_CODE  := MOTM

ARCH     := -mthumb -mthumb-interwork
CFLAGS   := -g -Wall -Wextra -O2 -mcpu=arm7tdmi -mtune=arm7tdmi $(ARCH) \
            -fomit-frame-pointer -std=gnu11 \
            -Iinclude -I$(GEN) -I$(DEVKITPRO)/libtonc/include
LDFLAGS  := -g $(ARCH) -specs=gba.specs -Wl,-Map,$(BUILD)/$(TARGET).map
LIBS     := -L$(DEVKITPRO)/libtonc/lib -ltonc

# ---------------------------------------------------------------------------
# Generated sources: assets/<name>.png (+ optional assets/<name>.opts holding
# extra png2gba flags) -> build/gen/gfx_<name>.c/.h
# ---------------------------------------------------------------------------
GFX_NAMES := $(basename $(notdir $(wildcard assets/*.png)))
GFX_SRCS  := $(patsubst %,$(GEN)/gfx_%.c,$(GFX_NAMES))
GFX_HDRS  := $(patsubst %,$(GEN)/gfx_%.h,$(GFX_NAMES))
# Word lists: data/<lang>_solutions.txt + data/<lang>_valid.txt -> wordlist_<lang>.c/.h
LANGS     := en fr es de it
WL_SRCS   := $(patsubst %,$(GEN)/wordlist_%.c,$(LANGS))
WL_HDRS   := $(patsubst %,$(GEN)/wordlist_%.h,$(LANGS))

SRCS := $(wildcard source/*.c)
OBJS := $(patsubst source/%.c,$(BUILD)/%.o,$(SRCS)) \
        $(patsubst $(GEN)/%.c,$(BUILD)/%.o,$(GFX_SRCS) $(WL_SRCS))

.PHONY: all clean run gen assets wordlists test emutest check
all: $(BUILD)/$(TARGET).gba

gen: $(GFX_SRCS) $(WL_SRCS)

# Refresh data/*.txt from the external word lists (needs network)
wordlists:
	$(PYTHON) tools/build_wordlists.py

# Redraw the source PNGs from the ASCII art in tools/make_assets.py
assets:
	$(PYTHON) tools/make_assets.py

$(BUILD)/$(TARGET).gba: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(GBAFIX) $@ -t$(GAME_TITLE) -c$(GAME_CODE) -m00
	@echo "built $@"

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

# Game sources depend on every generated header (cheap, keeps deps simple)
$(BUILD)/%.o: source/%.c $(GFX_HDRS) $(WL_HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: $(GEN)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(GEN)/gfx_%.c $(GEN)/gfx_%.h: assets/%.png tools/png2gba.py | $(GEN)
	$(PYTHON) tools/png2gba.py $< -o $(GEN)/gfx_$* $(shell cat assets/$*.opts 2>/dev/null)

$(GEN)/wordlist_%.c $(GEN)/wordlist_%.h: data/%_solutions.txt data/%_valid.txt tools/gen_wordlist.py | $(GEN)
	$(PYTHON) tools/gen_wordlist.py $* data/$*_solutions.txt data/$*_valid.txt -o $(GEN)/wordlist_$*

$(BUILD) $(GEN):
	mkdir -p $@

run: $(BUILD)/$(TARGET).gba
	$(MGBA) $< &

# ---------------------------------------------------------------------------
# Tests (see tests/README.md)
# ---------------------------------------------------------------------------
# Host unit tests: the game modules below compile on the PC against
# tests/unit/host_shim.h (fake registers and SRAM).
UNIT_GAME_SRCS := source/logic.c source/keyboard.c source/rng.c source/stats.c \
                  source/sound.c source/lang.c source/time_attack.c
UNIT_SRCS := $(wildcard tests/unit/*.c) $(UNIT_GAME_SRCS) $(WL_SRCS)
UNIT_FLAGS := -std=gnu11 -Wall -Wextra -O1 -g -DHOST_TEST -Iinclude -Itests/unit -I$(GEN)

$(BUILD)/unit_tests: $(UNIT_SRCS) $(wildcard include/*.h tests/unit/*.h) | $(BUILD)
	$(HOSTCC) $(UNIT_FLAGS) $(UNIT_SRCS) -o $@

test: $(BUILD)/unit_tests
	$(BUILD)/unit_tests $(TESTFLAGS)

# Emulator tests: tests/emu/run.py drives the ROM in mGBA through Lua scenarios.
#   make emutest                    every scenario
#   make emutest SCENARIO=04_classic_win
emutest: $(BUILD)/$(TARGET).gba
	$(PYTHON) tests/emu/run.py --rom $< $(SCENARIO)

check: test emutest

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)
