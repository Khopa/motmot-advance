# Wordle GBA — Makefile
# Build from an MSYS2 shell with devkitPro installed (DEVKITPRO=/opt/devkitpro):
#     make            -> build/wordle.gba
#     make run        -> launch in mGBA
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

SRCS := $(wildcard source/*.c)
OBJS := $(patsubst source/%.c,$(BUILD)/%.o,$(SRCS)) \
        $(patsubst $(GEN)/%.c,$(BUILD)/%.o,$(GFX_SRCS))

.PHONY: all clean run gen assets
all: $(BUILD)/$(TARGET).gba

gen: $(GFX_SRCS)

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
$(BUILD)/%.o: source/%.c $(GFX_HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: $(GEN)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(GEN)/gfx_%.c $(GEN)/gfx_%.h: assets/%.png tools/png2gba.py | $(GEN)
	$(PYTHON) tools/png2gba.py $< -o $(GEN)/gfx_$* $(shell cat assets/$*.opts 2>/dev/null)

$(BUILD) $(GEN):
	mkdir -p $@

run: $(BUILD)/$(TARGET).gba
	$(MGBA) $< &

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)
