# Wordle GBA — Makefile
# Build from an MSYS2 shell with devkitPro installed (DEVKITPRO=/opt/devkitpro):
#     make            -> build/wordle.gba
#     make run        -> launch in mGBA
#     make test       -> host-side unit tests of the game logic
#     make clean

TARGET   := wordle
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

GAME_TITLE := WORDLE
GAME_CODE  := WRDL

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
LANGS     := en fr
WL_SRCS   := $(patsubst %,$(GEN)/wordlist_%.c,$(LANGS))
WL_HDRS   := $(patsubst %,$(GEN)/wordlist_%.h,$(LANGS))

SRCS := $(wildcard source/*.c)
OBJS := $(patsubst source/%.c,$(BUILD)/%.o,$(SRCS)) \
        $(patsubst $(GEN)/%.c,$(BUILD)/%.o,$(GFX_SRCS) $(WL_SRCS))

.PHONY: all clean run gen assets wordlists test
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

test: | $(BUILD)
	$(HOSTCC) -std=gnu11 -Wall -Wextra -O1 -DHOST_TEST -Iinclude tests/test_logic.c source/logic.c -o $(BUILD)/test_logic
	$(BUILD)/test_logic

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)
