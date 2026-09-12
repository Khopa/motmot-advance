// Rendering on Mode 0 with three regular backgrounds and one sprite.
//   BG0  text        charblock 0 (font),        screenblock 28, priority 0
//   BG1  cells/keys  charblock 1 (cells, keys), screenblock 29, priority 1
//   BG2  title decor charblock 2 (pattern),     screenblock 30, priority 2
//   OBJ  cursor      obj tiles 0-3, obj palette banks 0/1
// Feedback colours are palette-bank swaps on the same tiles.
#include <string.h>
#include "render.h"
#include "keyboard.h"
#include "gfx_font.h"
#include "gfx_cells.h"
#include "gfx_keys.h"
#include "gfx_cursor.h"
#include "gfx_decor.h"

// Must match FONT_CHARS in tools/make_assets.py
static const char FONT_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?:.-/%><#',\x01\x02\x03\x04\x05";

#define CBB_TEXT   0
#define CBB_CELLS  1
#define CBB_DECOR  2
#define SBB_TEXT   28
#define SBB_CELLS  29
#define SBB_DECOR  30

// tile 0-3 of the cells charblock stay blank (empty map entries)
#define CELL_TILE_BASE 4
#define KEY_TILE_BASE  (CELL_TILE_BASE + cellsTileCount)
#define DECOR_TILE     1

#define CELL_META_COUNT 29      // " A-Z enter del" in cells.png / keys.png

// Full shadow OAM: oam_init() hides all 128 hardware entries (it works in
// groups of 4 and clears the interleaved affine matrices as well).
static OBJ_ATTR obj_buffer[128];
static u32 frame;

// --- colours ---------------------------------------------------------------
#define C_BACKDROP RGB15(2, 2, 2)       // #121213
#define C_DARK     RGB15(7, 7, 7)       // #3a3a3c
#define C_MID      RGB15(10, 10, 11)    // #565758
#define C_GRAY     RGB15(16, 16, 16)    // #818384
#define C_YELLOW   RGB15(22, 19, 7)     // #b59f3b
#define C_GREEN    RGB15(10, 17, 9)     // #538d4e
#define C_WHITE    RGB15(31, 31, 31)
#define C_DECOR    RGB15(4, 4, 4)

static void set_text_pal(int bank, u16 color)
{
    pal_bg_bank[bank][1] = color;
}

static void set_cell_pal(int bank, u16 fill, u16 border, u16 glyph)
{
    pal_bg_bank[bank][1] = fill;
    pal_bg_bank[bank][2] = border;
    pal_bg_bank[bank][3] = glyph;
}

void render_init(void)
{
    REG_DISPCNT = 0;    // blank while loading

    memcpy32(&tile_mem[CBB_TEXT][0], fontTiles, fontTilesLen / 4);
    memset32(&tile_mem[CBB_CELLS][0], 0, CELL_TILE_BASE * 8);
    memcpy32(&tile_mem[CBB_CELLS][CELL_TILE_BASE], cellsTiles, cellsTilesLen / 4);
    memcpy32(&tile_mem[CBB_CELLS][KEY_TILE_BASE], keysTiles, keysTilesLen / 4);
    memset32(&tile_mem[CBB_DECOR][0], 0, 8);
    memcpy32(&tile_mem[CBB_DECOR][DECOR_TILE], decorTiles, decorTilesLen / 4);
    memcpy32(&tile_mem_obj[0][0], cursorTiles, cursorTilesLen / 4);

    pal_bg_mem[0] = C_BACKDROP;
    set_text_pal(PAL_TXT_WHITE, C_WHITE);
    set_text_pal(PAL_TXT_GRAY, C_GRAY);
    set_text_pal(PAL_TXT_YELLOW, C_YELLOW);
    set_text_pal(PAL_TXT_GREEN, C_GREEN);
    set_text_pal(PAL_TXT_DIM, C_DARK);
    set_cell_pal(PAL_CELL_EMPTY, C_BACKDROP, C_DARK, C_WHITE);
    set_cell_pal(PAL_CELL_TYPED, C_BACKDROP, C_MID, C_WHITE);
    set_cell_pal(PAL_ABSENT, C_DARK, C_DARK, C_WHITE);
    set_cell_pal(PAL_PRESENT, C_YELLOW, C_YELLOW, C_WHITE);
    set_cell_pal(PAL_CORRECT, C_GREEN, C_GREEN, C_WHITE);
    set_cell_pal(PAL_KEY, C_GRAY, C_GRAY, C_WHITE);
    set_cell_pal(PAL_LOGO, C_BACKDROP, C_WHITE, C_WHITE);
    pal_bg_bank[PAL_DECOR][1] = C_DECOR;
    pal_obj_bank[0][1] = C_WHITE;
    pal_obj_bank[1][1] = C_GRAY;

    REG_BG0CNT = BG_CBB(CBB_TEXT)  | BG_SBB(SBB_TEXT)  | BG_4BPP | BG_REG_32x32 | BG_PRIO(0);
    REG_BG1CNT = BG_CBB(CBB_CELLS) | BG_SBB(SBB_CELLS) | BG_4BPP | BG_REG_32x32 | BG_PRIO(1);
    REG_BG2CNT = BG_CBB(CBB_DECOR) | BG_SBB(SBB_DECOR) | BG_4BPP | BG_REG_32x32 | BG_PRIO(2);

    oam_init(obj_buffer, 128);
    render_clear();

    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_BG1 | DCNT_BG2 | DCNT_OBJ | DCNT_OBJ_1D;
}

void render_vblank(void)
{
    frame++;
    // the cursor pulses gently between white and grey
    int bank = ((frame >> 4) & 1) ? 1 : 0;
    obj_buffer[0].attr2 = (obj_buffer[0].attr2 & ~ATTR2_PALBANK_MASK) | ATTR2_PALBANK(bank);
    oam_copy(oam_mem, obj_buffer, 1);
}

void render_clear(void)
{
    txt_clear();
    cells_clear();
    cells_scroll(0);
    decor_show(false);
    cursor_set(0, 0, false);
}

// --- text -------------------------------------------------------------------

static int font_index(char c)
{
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    for (int i = 0; FONT_CHARS[i]; i++)
        if (FONT_CHARS[i] == c) return i;
    return 0;
}

void txt_puts(int tx, int ty, const char *s, int pal)
{
    u16 *row = &se_mem[SBB_TEXT][ty * 32];
    for (; *s && tx < SCREEN_TW; s++, tx++)
        row[tx] = font_index(*s) | SE_PALBANK(pal);
}

void txt_center(int ty, const char *s, int pal)
{
    int len = strlen(s);
    txt_puts((SCREEN_TW - len) / 2, ty, s, pal);
}

int txt_uint(int tx, int ty, unsigned v, int pal)
{
    char buf[11];
    int n = 0;
    do {
        buf[n++] = '0' + v % 10;
        v /= 10;
    } while (v);
    for (int i = 0; i < n; i++)
        se_mem[SBB_TEXT][ty * 32 + tx + i] = font_index(buf[n - 1 - i]) | SE_PALBANK(pal);
    return n;
}

void txt_fill(int tx, int ty, int n, char c, int pal)
{
    u16 e = font_index(c) | SE_PALBANK(pal);
    for (int i = 0; i < n && tx + i < SCREEN_TW; i++)
        se_mem[SBB_TEXT][ty * 32 + tx + i] = e;
}

void txt_clear_row(int ty)
{
    memset32(&se_mem[SBB_TEXT][ty * 32], 0, 16);
}

void txt_clear(void)
{
    memset32(se_mem[SBB_TEXT], 0, 32 * 32 / 2);
}

// --- cells / keys -----------------------------------------------------------

static int cell_meta(char ch)
{
    if (ch >= 'A' && ch <= 'Z') return 1 + (ch - 'A');
    if (ch == KEY_ENTER) return 27;
    if (ch == KEY_DEL)   return 28;
    return 0;
}

static void meta_draw(int tx, int ty, int tile, int pal)
{
    u16 *map = se_mem[SBB_CELLS];
    map[ty * 32 + tx]           = (tile + 0) | SE_PALBANK(pal);
    map[ty * 32 + tx + 1]       = (tile + 1) | SE_PALBANK(pal);
    map[(ty + 1) * 32 + tx]     = (tile + 2) | SE_PALBANK(pal);
    map[(ty + 1) * 32 + tx + 1] = (tile + 3) | SE_PALBANK(pal);
}

void cell_draw(int tx, int ty, char ch, int pal)
{
    meta_draw(tx, ty, CELL_TILE_BASE + cell_meta(ch) * 4, pal);
}

void key_draw(int tx, int ty, char ch, int pal)
{
    meta_draw(tx, ty, KEY_TILE_BASE + cell_meta(ch) * 4, pal);
}

void cells_clear(void)
{
    memset32(se_mem[SBB_CELLS], 0, 32 * 32 / 2);
}

void cells_scroll(int px)
{
    REG_BG1HOFS = px;
}

// --- decor ------------------------------------------------------------------

void decor_show(bool on)
{
    u32 e = on ? (DECOR_TILE | SE_PALBANK(PAL_DECOR)) : 0;
    memset32(se_mem[SBB_DECOR], e | (e << 16), 32 * 32 / 2);
}

// --- cursor -----------------------------------------------------------------

void cursor_set(int px, int py, bool visible)
{
    obj_set_attr(&obj_buffer[0],
                 ATTR0_SQUARE | ATTR0_4BPP | (visible ? 0 : ATTR0_HIDE) | ATTR0_Y(py),
                 ATTR1_SIZE_16 | ATTR1_X(px),
                 ATTR2_PALBANK(0) | 0);
}

// --- game screen helpers ----------------------------------------------------

int fb_to_pal(u8 fb)
{
    switch (fb) {
    case FB_ABSENT:  return PAL_ABSENT;
    case FB_PRESENT: return PAL_PRESENT;
    case FB_CORRECT: return PAL_CORRECT;
    default:         return PAL_CELL_TYPED;
    }
}

// Draw one grid row. For a committed row, only the first `cols_revealed`
// cells show their colour (used by the reveal animation).
void render_grid_row(const GameState *g, int row, int cols_revealed)
{
    int ty = GRID_TY + row * 2;
    for (int c = 0; c < WORD_LEN; c++) {
        int tx = GRID_TX + c * 2;
        if (row < g->n_guesses) {
            char ch = g->guesses[row][c];
            int pal = c < cols_revealed ? fb_to_pal(g->feedback[row][c]) : PAL_CELL_TYPED;
            cell_draw(tx, ty, ch, pal);
        } else if (row == g->n_guesses && c < g->cur_len && g->status == STATUS_PLAYING) {
            cell_draw(tx, ty, g->current[c], PAL_CELL_TYPED);
        } else {
            cell_draw(tx, ty, ' ', PAL_CELL_EMPTY);
        }
    }
}

void render_grid(const GameState *g)
{
    for (int r = 0; r < MAX_GUESSES; r++)
        render_grid_row(g, r, WORD_LEN);
}

void render_keyboard(const GameState *g, const Language *lang)
{
    for (int r = 0; r < KB_ROWS; r++) {
        const char *keys = lang->kb_rows[r];
        for (int k = 0; keys[k]; k++) {
            int tx, ty;
            kb_key_pos(lang, r, k, &tx, &ty);
            char ch = keys[k];
            int pal = PAL_KEY;
            if (ch >= 'A' && ch <= 'Z' && g->key_state[ch - 'A'] != FB_NONE)
                pal = fb_to_pal(g->key_state[ch - 'A']);
            key_draw(tx, ty, ch, pal);
        }
    }
}
