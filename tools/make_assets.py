#!/usr/bin/env python3
"""Draw the source PNG images used by the game (font, grid cells, keyboard
keys, cursor sprite, title decoration).

The pixel art is defined here as ASCII art so it is versioned as text and can
be tweaked easily. Run once to (re)generate assets/*.png; png2gba.py turns
those PNGs into GBA tile data at build time.

Every image is written as an indexed (mode "P") PNG whose palette indices are
what the game expects:
    0 = transparent, 1 = fill, 2 = border, 3 = glyph   (cells / keys)
    0 = transparent, 1 = ink                           (font / cursor / decor)
The actual colours are chosen at run time by palette-bank swapping, so the
PNG palette is only a preview.
"""
import os
import sys
from PIL import Image

ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets")

# Glyph order in font.png; the C side (render.c FONT_CHARS) must match.
# \x01 = enter icon, \x02 = backspace icon, \x03 = bar segment (stat bars),
# \x04 = full heart, \x05 = empty heart (Marathon lives)
FONT_CHARS = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?:.-/%><#',\x01\x02\x03\x04\x05"

GLYPHS = {
" ": """
......
......
......
......
......
......
......
""",
"A": """
.####.
#....#
#....#
######
#....#
#....#
#....#
""",
"B": """
#####.
#....#
#....#
#####.
#....#
#....#
#####.
""",
"C": """
.####.
#....#
#.....
#.....
#.....
#....#
.####.
""",
"D": """
#####.
#....#
#....#
#....#
#....#
#....#
#####.
""",
"E": """
######
#.....
#.....
#####.
#.....
#.....
######
""",
"F": """
######
#.....
#.....
#####.
#.....
#.....
#.....
""",
"G": """
.####.
#....#
#.....
#..###
#....#
#....#
.####.
""",
"H": """
#....#
#....#
#....#
######
#....#
#....#
#....#
""",
"I": """
.####.
..##..
..##..
..##..
..##..
..##..
.####.
""",
"J": """
..####
....#.
....#.
....#.
....#.
#...#.
.###..
""",
"K": """
#....#
#...#.
#..#..
###...
#..#..
#...#.
#....#
""",
"L": """
#.....
#.....
#.....
#.....
#.....
#.....
######
""",
"M": """
#....#
##..##
#.##.#
#.##.#
#....#
#....#
#....#
""",
"N": """
#....#
##...#
#.#..#
#..#.#
#...##
#....#
#....#
""",
"O": """
.####.
#....#
#....#
#....#
#....#
#....#
.####.
""",
"P": """
#####.
#....#
#....#
#####.
#.....
#.....
#.....
""",
"Q": """
.####.
#....#
#....#
#....#
#..#.#
#...#.
.###.#
""",
"R": """
#####.
#....#
#....#
#####.
#..#..
#...#.
#....#
""",
"S": """
.#####
#.....
#.....
.####.
.....#
.....#
#####.
""",
"T": """
######
..##..
..##..
..##..
..##..
..##..
..##..
""",
"U": """
#....#
#....#
#....#
#....#
#....#
#....#
.####.
""",
"V": """
#....#
#....#
#....#
#....#
.#..#.
.#..#.
..##..
""",
"W": """
#....#
#....#
#....#
#.##.#
#.##.#
##..##
#....#
""",
"X": """
#....#
#....#
.#..#.
..##..
.#..#.
#....#
#....#
""",
"Y": """
#....#
#....#
.#..#.
..##..
..##..
..##..
..##..
""",
"Z": """
######
.....#
....#.
...#..
..#...
.#....
######
""",
"0": """
.####.
#....#
#...##
#.#..#
##...#
#....#
.####.
""",
"1": """
..##..
.###..
..##..
..##..
..##..
..##..
######
""",
"2": """
.####.
#....#
.....#
...##.
..#...
.#....
######
""",
"3": """
.####.
#....#
.....#
..###.
.....#
#....#
.####.
""",
"4": """
....#.
...##.
..#.#.
.#..#.
######
....#.
....#.
""",
"5": """
######
#.....
#.....
#####.
.....#
#....#
.####.
""",
"6": """
.####.
#.....
#.....
#####.
#....#
#....#
.####.
""",
"7": """
######
.....#
....#.
...#..
..#...
..#...
..#...
""",
"8": """
.####.
#....#
#....#
.####.
#....#
#....#
.####.
""",
"9": """
.####.
#....#
#....#
.#####
.....#
.....#
.####.
""",
"!": """
..##..
..##..
..##..
..##..
..##..
......
..##..
""",
"?": """
.####.
#....#
.....#
...##.
..#...
......
..#...
""",
":": """
......
..##..
..##..
......
..##..
..##..
......
""",
".": """
......
......
......
......
......
..##..
..##..
""",
"-": """
......
......
......
.####.
......
......
......
""",
"/": """
.....#
....#.
...#..
..#...
.#....
#.....
......
""",
"%": """
##...#
##..#.
...#..
..#...
.#....
#..##.
#..##.
""",
">": """
#.....
.#....
..#...
...#..
..#...
.#....
#.....
""",
"<": """
...#..
..#...
.#....
#.....
.#....
..#...
...#..
""",
"#": """
.#..#.
.#..#.
######
.#..#.
######
.#..#.
.#..#.
""",
"'": """
..##..
..##..
...#..
......
......
......
......
""",
",": """
......
......
......
......
..##..
..##..
.#....
""",
# enter icon (return arrow)
"\x01": """
.....#
.....#
..#..#
.#...#
######
.#....
..#...
""",
# backspace icon (left arrow)
"\x02": """
......
..#...
.#....
######
.#....
..#...
......
""",
# bar segment (stat bars); drawn 8 px wide by blit_glyph
"\x03": """
######
######
######
######
######
######
######
""",
# full heart
"\x04": """
.#..#.
######
######
######
.####.
..##..
......
""",
# empty heart
"\x05": """
.#..#.
#.##.#
#....#
#....#
.#..#.
..##..
......
""",
}

GLYPH_W, GLYPH_H = 6, 7


def glyph_rows(ch):
    rows = [r for r in GLYPHS[ch].strip("\n").split("\n")]
    assert len(rows) == GLYPH_H, f"glyph {ch!r}: {len(rows)} rows"
    for r in rows:
        assert len(r) == GLYPH_W, f"glyph {ch!r}: bad row {r!r}"
    return rows


def blit_glyph(px, ch, x0, y0, color):
    if ch == "\x03":                  # bar segment: full width, rows 1-6
        for y in range(1, 7):
            for x in range(8):
                px[x0 - 1 + x, y0 + y] = color
        return
    for y, row in enumerate(glyph_rows(ch)):
        for x, c in enumerate(row):
            if c == "#":
                px[x0 + x, y0 + y] = color


def new_indexed(w, h, palette):
    img = Image.new("P", (w, h), 0)
    flat = []
    for rgb in palette:
        flat += list(rgb)
    flat += [0, 0, 0] * (256 - len(palette))
    img.putpalette(flat)
    return img


PREVIEW_PAL = [
    (255, 0, 255),    # 0 transparent (magenta for preview)
    (18, 18, 19),     # 1 fill
    (86, 87, 88),     # 2 border
    (255, 255, 255),  # 3 glyph
]


def make_font():
    """8x8 glyph strip, one tile per character of FONT_CHARS (ink = 1)."""
    n = len(FONT_CHARS)
    img = new_indexed(8 * n, 8, [(255, 0, 255), (255, 255, 255)])
    px = img.load()
    for i, ch in enumerate(FONT_CHARS):
        blit_glyph(px, ch, i * 8 + 1, 0, 1)
    img.save(os.path.join(ASSETS, "font.png"))


CELL_CHARS = " ABCDEFGHIJKLMNOPQRSTUVWXYZ\x01\x02"   # 29 metatiles


def draw_box(px, x0, y0, size, rounded):
    """size x size box at (x0,y0): border = 2, inside = 1."""
    for y in range(size):
        for x in range(size):
            on_edge = x == 0 or y == 0 or x == size - 1 or y == size - 1
            corner = rounded and (x in (0, size - 1)) and (y in (0, size - 1))
            if corner:
                continue                      # leave transparent: rounded look
            px[x0 + x, y0 + y] = 2 if on_edge else 1


def make_cells(name, rounded):
    """16x16 metatiles: a 14x14 box (1px transparent margin) with the glyph
    centred. Order = CELL_CHARS. Indices: 1 fill, 2 border, 3 glyph."""
    n = len(CELL_CHARS)
    img = new_indexed(16 * n, 16, PREVIEW_PAL)
    px = img.load()
    for i, ch in enumerate(CELL_CHARS):
        x0 = i * 16
        draw_box(px, x0 + 1, 1, 14, rounded)
        # centre the 6x7 glyph in the 16x16 metatile
        blit_glyph(px, ch, x0 + (16 - GLYPH_W) // 2, (16 - GLYPH_H) // 2, 3)
    img.save(os.path.join(ASSETS, name))


CURSOR = """
..############..
.##############.
################
###..........###
##............##
##............##
##............##
##............##
##............##
##............##
##............##
##............##
###..........###
################
.##############.
..############..
"""


def make_cursor():
    """16x16 sprite: 2px rounded frame, ink = 1."""
    img = new_indexed(16, 16, [(255, 0, 255), (255, 255, 255)])
    px = img.load()
    rows = CURSOR.strip("\n").split("\n")
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            if c == "#":
                px[x, y] = 1
    img.save(os.path.join(ASSETS, "cursor.png"))


DECOR = """
#.......
........
........
........
#...#...
........
........
........
"""


def make_decor():
    """8x8 background pattern for the title screen (ink = 1)."""
    img = new_indexed(8, 8, [(18, 18, 19), (58, 58, 60)])
    px = img.load()
    rows = DECOR.strip("\n").split("\n")
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            px[x, y] = 1 if c == "#" else 0
    img.save(os.path.join(ASSETS, "decor.png"))


def main():
    os.makedirs(ASSETS, exist_ok=True)
    make_font()
    make_cells("cells.png", rounded=False)
    make_cells("keys.png", rounded=True)
    make_cursor()
    make_decor()
    print("assets written to", os.path.normpath(ASSETS))


if __name__ == "__main__":
    sys.exit(main())
