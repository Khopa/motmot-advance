// Drawing: text layer (BG0), cells/keys layer (BG1), title pattern (BG2),
// cursor sprite. Coordinates are in 8x8 tiles unless stated otherwise.
#ifndef RENDER_H
#define RENDER_H

#include "common.h"
#include "game_state.h"
#include "lang.h"
#include "layout.h"

// Palette banks (BG). Text banks use colour index 1; cell/key banks use
// 1 = fill, 2 = border, 3 = glyph.
enum {
    PAL_TXT_WHITE = 1,
    PAL_TXT_GRAY,
    PAL_TXT_YELLOW,
    PAL_TXT_GREEN,
    PAL_TXT_DIM,
    PAL_CELL_EMPTY,
    PAL_CELL_TYPED,
    PAL_ABSENT,
    PAL_PRESENT,
    PAL_CORRECT,
    PAL_KEY,
    PAL_DECOR,
    PAL_LOGO,        // title logo cells: white border on dark
    PAL_TXT_BOX,     // opaque modal background
};

void render_init(void);
void render_vblank(void);        // commit OAM; call right after VBlankIntrWait
void render_clear(void);         // wipe every layer, hide the cursor

// text (BG0)
void txt_puts(int tx, int ty, const char *s, int pal);
int  txt_center(int ty, const char *s, int pal);        // pixel-exact; returns the column
void txt_center_marked(int ty, const char *s, int pal, bool selected);  // "> s <" when selected
int  txt_uint(int tx, int ty, unsigned v, int pal);     // returns width
void txt_fill(int tx, int ty, int n, char c, int pal);  // n copies of c
void txt_clear_row(int ty);
void txt_clear(void);

// 16x16 cells and keys (BG1), tile coordinates of the top-left tile
void cell_draw(int tx, int ty, char ch, int pal);
void key_draw(int tx, int ty, char ch, int pal);
void cells_clear(void);

// title pattern (BG2)
void decor_show(bool on);

// modal box covering the middle of the screen (grid included), two text lines
void modal_show(const char *line1, const char *line2);
void modal_hide(void);

// cursor sprite, pixel coordinates
void cursor_set(int px, int py, bool visible);

// composite drawing helpers for the game screen
int  fb_to_pal(u8 fb);
void render_grid_row(const GameState *g, int row, int cols_revealed);
void render_grid(const GameState *g);
void render_keyboard(const GameState *g, const Language *lang);

#endif
