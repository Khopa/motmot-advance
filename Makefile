# Wordle GBA — Makefile
# Build from an MSYS2 shell with devkitPro installed (DEVKITPRO=/opt/devkitpro):
#     make            -> build/wordle.gba
#     make run        -> launch in mGBA
#     make clean

TARGET   := wordle
BUILD    := build

DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
PREFIX   := $(DEVKITARM)/bin/arm-none-eabi-
CC       := $(PREFIX)gcc
OBJCOPY  := $(PREFIX)objcopy
GBAFIX   := $(DEVKITPRO)/tools/bin/gbafix
MGBA     ?= /c/Program\ Files/mGBA/mGBA.exe

GAME_TITLE := WORDLE
GAME_CODE  := WRDL

ARCH     := -mthumb -mthumb-interwork
CFLAGS   := -g -Wall -Wextra -O2 -mcpu=arm7tdmi -mtune=arm7tdmi $(ARCH) \
            -fomit-frame-pointer -std=gnu11 \
            -Iinclude -I$(DEVKITPRO)/libtonc/include
LDFLAGS  := -g $(ARCH) -specs=gba.specs -Wl,-Map,$(BUILD)/$(TARGET).map
LIBS     := -L$(DEVKITPRO)/libtonc/lib -ltonc

SRCS := $(wildcard source/*.c)
OBJS := $(patsubst source/%.c,$(BUILD)/%.o,$(SRCS))

.PHONY: all clean run
all: $(BUILD)/$(TARGET).gba

$(BUILD)/$(TARGET).gba: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(GBAFIX) $@ -t$(GAME_TITLE) -c$(GAME_CODE) -m00
	@echo "built $@"

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD)/%.o: source/%.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD):
	mkdir -p $@

run: $(BUILD)/$(TARGET).gba
	$(MGBA) $< &

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)
