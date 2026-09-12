#include <string.h>
#include "keyboard.h"
#include "render.h"

int kb_row_len(const Language *lang, int row)
{
    return strlen(lang->kb_rows[row]);
}

char kb_key_at(const Language *lang, const KbCursor *c)
{
    return lang->kb_rows[c->row][c->col];
}

void kb_move(const Language *lang, KbCursor *c, int dx, int dy)
{
    if (dx) {
        int n = kb_row_len(lang, c->row);
        c->col = (c->col + dx + n) % n;
    }
    if (dy) {
        int new_row = (c->row + dy + KB_ROWS) % KB_ROWS;
        // keep the same horizontal screen position: rows are centred, so
        // convert the current key's tile x into a column of the new row
        int tx, ty, tx0, ty0;
        kb_key_pos(lang, c->row, c->col, &tx, &ty);
        kb_key_pos(lang, new_row, 0, &tx0, &ty0);
        int col = (tx - tx0 + 1) >> 1;     // round to nearest key
        int n = kb_row_len(lang, new_row);
        if (col < 0) col = 0;
        if (col >= n) col = n - 1;
        c->row = new_row;
        c->col = col;
    }
}
