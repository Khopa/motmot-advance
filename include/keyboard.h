// Virtual keyboard cursor: navigation over the language's key layout.
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "common.h"
#include "lang.h"

typedef struct {
    u8 row, col;
} KbCursor;

int  kb_row_len(const Language *lang, int row);
char kb_key_at(const Language *lang, const KbCursor *c);
// dx/dy in {-1,0,1}; horizontal moves wrap, vertical moves keep the
// screen column as close as possible
void kb_move(const Language *lang, KbCursor *c, int dx, int dy);

#endif
