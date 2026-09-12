#include <string.h>
#include "time_attack.h"

const u8 ta_word_counts[TA_LENGTH_COUNT] = { 5, 10, 15 };

static void two_digits(char *p, unsigned v)
{
    p[0] = '0' + (v / 10) % 10;
    p[1] = '0' + v % 10;
}

void ta_format_time(u32 frames, char out[9])
{
    if (frames > TA_MAX_FRAMES) frames = TA_MAX_FRAMES;
    unsigned centis = (frames % 60) * 100 / 60;
    unsigned seconds = frames / 60;
    two_digits(out, seconds / 60);
    out[2] = ':';
    two_digits(out + 3, seconds % 60);
    out[5] = '.';
    two_digits(out + 6, centis);
    out[8] = 0;
}

int ta_board_rank(const TimeRecord board[TA_TOP], u32 frames)
{
    for (int i = 0; i < TA_TOP; i++)
        if (!board[i].used || frames < board[i].frames)
            return i;
    return -1;
}

int ta_board_insert(TimeRecord board[TA_TOP], u32 frames, const char initials[3])
{
    int rank = ta_board_rank(board, frames);
    if (rank < 0) return -1;
    for (int i = TA_TOP - 1; i > rank; i--)
        board[i] = board[i - 1];
    board[rank].frames = frames;
    memcpy(board[rank].initials, initials, 3);
    board[rank].used = 1;
    return rank;
}
