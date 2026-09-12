// Virtual keyboard: row lengths, screen positions, cursor navigation.
#include "test.h"
#include "keyboard.h"
#include "layout.h"

static const Language azerty = { .kb_rows = { "AZERTYUIOP", "QSDFGHJKLM", "\x01WXCVBN\x02" } };
static const Language qwerty = { .kb_rows = { "QWERTYUIOP", "ASDFGHJKL", "\x01ZXCVBNM\x02" } };

TEST(row_lengths)
{
    CHECK_EQ(kb_row_len(&azerty, 0), 10);
    CHECK_EQ(kb_row_len(&azerty, 1), 10);
    CHECK_EQ(kb_row_len(&azerty, 2), 8);
    CHECK_EQ(kb_row_len(&qwerty, 1), 9);
    CHECK_EQ(kb_row_len(&qwerty, 2), 9);
}

TEST(key_at_cursor)
{
    KbCursor c = { 0, 0 };
    CHECK_EQ(kb_key_at(&azerty, &c), 'A');
    CHECK_EQ(kb_key_at(&qwerty, &c), 'Q');
    c.row = 2; c.col = 0;
    CHECK_EQ(kb_key_at(&azerty, &c), KEY_ENTER);
    c.col = 7;
    CHECK_EQ(kb_key_at(&azerty, &c), KEY_DEL);
    c.col = 8;
    CHECK_EQ(kb_key_at(&qwerty, &c), KEY_DEL);
}

TEST(rows_are_centred_on_screen)
{
    int tx, ty;
    kb_key_pos(&azerty, 0, 0, &tx, &ty);         // 10 keys = 20 tiles -> starts at 5
    CHECK_EQ(tx, 5);
    CHECK_EQ(ty, KB_TY);
    kb_key_pos(&azerty, 0, 9, &tx, &ty);
    CHECK_EQ(tx, 23);
    kb_key_pos(&qwerty, 1, 0, &tx, &ty);         // 9 keys = 18 tiles -> starts at 6
    CHECK_EQ(tx, 6);
    CHECK_EQ(ty, KB_TY + 2);
    kb_key_pos(&azerty, 2, 0, &tx, &ty);         // 8 keys = 16 tiles -> starts at 7
    CHECK_EQ(tx, 7);
    CHECK_EQ(ty, KB_TY + 4);
    // every key fits on the 30-tile screen
    for (int r = 0; r < KB_ROWS; r++)
        for (int k = 0; k < kb_row_len(&qwerty, r); k++) {
            kb_key_pos(&qwerty, r, k, &tx, &ty);
            CHECK(tx >= 0 && tx + 2 <= SCREEN_TW);
            CHECK(ty + 2 <= SCREEN_TH);
        }
}

TEST(horizontal_moves_wrap)
{
    KbCursor c = { 0, 0 };
    kb_move(&azerty, &c, -1, 0);
    CHECK_EQ(c.col, 9);
    kb_move(&azerty, &c, 1, 0);
    CHECK_EQ(c.col, 0);
    for (int i = 0; i < 10; i++) kb_move(&azerty, &c, 1, 0);
    CHECK_EQ(c.col, 0);
    CHECK_EQ(c.row, 0);
}

TEST(vertical_moves_wrap)
{
    KbCursor c = { 0, 0 };
    kb_move(&azerty, &c, 0, -1);
    CHECK_EQ(c.row, 2);
    kb_move(&azerty, &c, 0, 1);
    CHECK_EQ(c.row, 0);
    kb_move(&azerty, &c, 0, 1);
    kb_move(&azerty, &c, 0, 1);
    CHECK_EQ(c.row, 2);
    kb_move(&azerty, &c, 0, 1);
    CHECK_EQ(c.row, 0);
}

TEST(vertical_move_keeps_screen_column)
{
    // AZERTY rows 0 and 1 have the same length: column is kept as is
    KbCursor c = { 0, 7 };
    kb_move(&azerty, &c, 0, 1);
    CHECK_EQ(c.row, 1);
    CHECK_EQ(c.col, 7);
    // row 1 (10 keys, starts at tile 5) -> row 2 (8 keys, starts at tile 7):
    // key 7 of row 1 is at tile 19, nearest key of row 2 is (19 - 7 + 1) / 2 = 6
    kb_move(&azerty, &c, 0, 1);
    CHECK_EQ(c.row, 2);
    CHECK_EQ(c.col, 6);
    // QWERTY: from row 0 col 0 (tile 5) down to row 1 (starts at 6): col 0
    KbCursor q = { 0, 0 };
    kb_move(&qwerty, &q, 0, 1);
    CHECK_EQ(q.col, 0);
    // from the last key of row 0 (tile 23) to row 1: (23 - 6 + 1) / 2 = 9 -> clamped to 8
    q.row = 0; q.col = 9;
    kb_move(&qwerty, &q, 0, 1);
    CHECK_EQ(q.col, 8);
    // and up from there wraps to row 0: tile 22 -> (22 - 5 + 1) / 2 = 9
    kb_move(&qwerty, &q, 0, -1);
    CHECK_EQ(q.row, 0);
    CHECK_EQ(q.col, 9);
}

TEST(vertical_move_clamps_to_shorter_row)
{
    KbCursor c = { 0, 9 };                       // last key of the top AZERTY row (tile 23)
    kb_move(&azerty, &c, 0, -1);                 // up wraps to row 2 (8 keys, tiles 7..21)
    CHECK_EQ(c.row, 2);
    CHECK_EQ(c.col, 7);
}

TEST(diagonal_move_applies_both_axes)
{
    KbCursor c = { 0, 0 };
    kb_move(&azerty, &c, 1, 1);
    CHECK_EQ(c.row, 1);
    CHECK_EQ(c.col, 1);
}

TEST(no_move_is_a_no_op)
{
    KbCursor c = { 1, 4 };
    kb_move(&azerty, &c, 0, 0);
    CHECK_EQ(c.row, 1);
    CHECK_EQ(c.col, 4);
}

static const TestCase keyboard_tests[] = {
    T(row_lengths), T(key_at_cursor), T(rows_are_centred_on_screen),
    T(horizontal_moves_wrap), T(vertical_moves_wrap), T(vertical_move_keeps_screen_column),
    T(vertical_move_clamps_to_shorter_row), T(diagonal_move_applies_both_axes), T(no_move_is_a_no_op),
};
SUITE(keyboard)
