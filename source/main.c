// Wordle GBA — entry point (tile pipeline test)
#include <tonc.h>
#include "gfx_font.h"
#include "gfx_cells.h"
#include "gfx_keys.h"

#define FONT_CHARS " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?:.-/%><#',\x01\x02"

static int font_index(char c)
{
    for (int i = 0; FONT_CHARS[i]; i++)
        if (FONT_CHARS[i] == c) return i;
    return 0;
}

int main(void)
{
    irq_init(NULL);
    irq_enable(II_VBLANK);

    // BG0: text (charblock 0, screenblock 28); BG1: cells (charblock 1, sb 29)
    REG_BG0CNT = BG_CBB(0) | BG_SBB(28) | BG_4BPP | BG_REG_32x32 | BG_PRIO(0);
    REG_BG1CNT = BG_CBB(1) | BG_SBB(29) | BG_4BPP | BG_REG_32x32 | BG_PRIO(1);
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_BG1;

    memcpy32(&tile_mem[0][0], fontTiles, fontTilesLen / 4);
    memcpy32(&tile_mem[1][0], cellsTiles, cellsTilesLen / 4);
    memcpy32(&tile_mem[1][cellsTileCount], keysTiles, keysTilesLen / 4);

    pal_bg_mem[0] = RGB15(2, 2, 2);
    // bank 1: text white
    pal_bg_bank[1][1] = CLR_WHITE;
    // bank 2: empty cell, bank 3: typed, 4: absent, 5: present, 6: correct, 7: key
    pal_bg_bank[2][1] = RGB15(2, 2, 2);   pal_bg_bank[2][2] = RGB15(7, 7, 7);   pal_bg_bank[2][3] = CLR_WHITE;
    pal_bg_bank[3][1] = RGB15(2, 2, 2);   pal_bg_bank[3][2] = RGB15(11, 11, 11); pal_bg_bank[3][3] = CLR_WHITE;
    pal_bg_bank[4][1] = RGB15(7, 7, 7);   pal_bg_bank[4][2] = RGB15(7, 7, 7);   pal_bg_bank[4][3] = CLR_WHITE;
    pal_bg_bank[5][1] = RGB15(22, 19, 7); pal_bg_bank[5][2] = RGB15(22, 19, 7); pal_bg_bank[5][3] = CLR_WHITE;
    pal_bg_bank[6][1] = RGB15(10, 17, 9); pal_bg_bank[6][2] = RGB15(10, 17, 9); pal_bg_bank[6][3] = CLR_WHITE;
    pal_bg_bank[7][1] = RGB15(16, 16, 16); pal_bg_bank[7][2] = RGB15(16, 16, 16); pal_bg_bank[7][3] = CLR_WHITE;

    const char *msg = "WORDLE GBA 0123 !?";
    for (int i = 0; msg[i]; i++)
        se_mem[28][1 * 32 + 6 + i] = font_index(msg[i]) | SE_PALBANK(1);

    const char *word = "CRANE";
    int pals[5] = {6, 5, 4, 3, 2};
    for (int c = 0; c < 5; c++) {
        int meta = word[c] - 'A' + 1;
        if (c == 4) meta = 0;
        int base = meta * 4;
        int x = 10 + c * 2, y = 4;
        se_mem[29][y * 32 + x]           = (base + 0) | SE_PALBANK(pals[c]);
        se_mem[29][y * 32 + x + 1]       = (base + 1) | SE_PALBANK(pals[c]);
        se_mem[29][(y + 1) * 32 + x]     = (base + 2) | SE_PALBANK(pals[c]);
        se_mem[29][(y + 1) * 32 + x + 1] = (base + 3) | SE_PALBANK(pals[c]);
    }
    const char *row = "QWERTYUIOP";
    for (int c = 0; c < 10; c++) {
        int base = cellsTileCount + (row[c] - 'A' + 1) * 4;
        int x = 5 + c * 2, y = 14;
        se_mem[29][y * 32 + x]           = (base + 0) | SE_PALBANK(7);
        se_mem[29][y * 32 + x + 1]       = (base + 1) | SE_PALBANK(7);
        se_mem[29][(y + 1) * 32 + x]     = (base + 2) | SE_PALBANK(7);
        se_mem[29][(y + 1) * 32 + x + 1] = (base + 3) | SE_PALBANK(7);
    }
    // enter / del keys
    for (int k = 0; k < 2; k++) {
        int base = cellsTileCount + (27 + k) * 4;
        int x = 8 + k * 4, y = 17;
        se_mem[29][y * 32 + x]           = (base + 0) | SE_PALBANK(7);
        se_mem[29][y * 32 + x + 1]       = (base + 1) | SE_PALBANK(7);
        se_mem[29][(y + 1) * 32 + x]     = (base + 2) | SE_PALBANK(7);
        se_mem[29][(y + 1) * 32 + x + 1] = (base + 3) | SE_PALBANK(7);
    }

    while (1)
        VBlankIntrWait();
    return 0;
}
