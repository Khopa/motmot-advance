// State of one round: target word, committed guesses and their
// feedback, the row being typed, and the per-letter keyboard colours.
#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "common.h"

typedef enum {
    FB_NONE = 0,    // not evaluated yet
    FB_ABSENT,      // grey: letter not in the word
    FB_PRESENT,     // yellow: letter in the word, wrong position
    FB_CORRECT,     // green: right letter, right position
} Feedback;

typedef enum { MODE_CLASSIC = 0, MODE_MARATHON = 1, MODE_TIME_ATTACK = 2 } GameMode;

typedef enum { DIFF_EASY = 0, DIFF_HARD = 1, DIFF_COUNT } Difficulty;   // Marathon

// A Marathon run: chained random words, lives, high score per difficulty
typedef struct {
    u8   difficulty;                // Difficulty
    u8   hp, hp_max;
    u16  score;                     // words found
    bool new_record;
} MarathonState;

// A Time Attack run: `total` words as fast as possible, +30 s per missed word
typedef struct {
    u8   length_idx;                // index into ta_word_counts
    u8   total;
    u8   done;                      // words finished (found or missed)
    u8   missed;
    u32  frames;                    // running time incl. penalties
    bool running;                   // timer active
    s8   rank;                      // leaderboard rank of the finished run, -1 if none
} TimeAttackState;

typedef enum { STATUS_PLAYING = 0, STATUS_WON, STATUS_LOST } GameStatus;

typedef struct {
    u8   lang;                              // LangId
    u8   mode;                              // GameMode
    u8   status;                            // GameStatus
    char target[WORD_LEN];
    u8   n_guesses;                         // committed rows
    char guesses[MAX_GUESSES][WORD_LEN];
    u8   feedback[MAX_GUESSES][WORD_LEN];   // Feedback per cell
    u8   cur_len;                           // letters typed in the current row
    char current[WORD_LEN];
    u8   key_state[26];                     // best Feedback seen per letter A-Z
} GameState;

#endif
