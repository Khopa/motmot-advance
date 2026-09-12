// Time Attack: solve a fixed number of words as fast as possible.
// Pure helpers (no hardware): time formatting and the leaderboards.
#ifndef TIME_ATTACK_H
#define TIME_ATTACK_H

#include "common.h"

#define TA_LENGTH_COUNT   3         // 5, 10 or 15 words
#define TA_TOP            3         // entries per leaderboard
#define TA_PENALTY_FRAMES (30 * 60) // a missed word costs 30 seconds
#define TA_MAX_FRAMES     (u32)(100 * 60 * 60 - 1)   // 99:59.98 display cap

extern const u8 ta_word_counts[TA_LENGTH_COUNT];

typedef struct {
    u32  frames;        // total time, 60 per second (penalties included)
    char initials[3];
    u8   used;          // 0 = empty slot
} TimeRecord;

// "MM:SS.CC" (9 bytes with the terminator); "--:--.--" for an empty record
void ta_format_time(u32 frames, char out[9]);

// Rank (0 = best) the time would take on the board, or -1 if not in the top
int  ta_board_rank(const TimeRecord board[TA_TOP], u32 frames);
// Insert if it ranks; returns the rank or -1
int  ta_board_insert(TimeRecord board[TA_TOP], u32 frames, const char initials[3]);

#endif
