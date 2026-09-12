// Language table: word lists, keyboard layout and UI strings per language.
#ifndef LANG_H
#define LANG_H

#include "common.h"

typedef enum { LANG_FR = 0, LANG_EN = 1, LANG_COUNT } LangId;

// Special key codes used in keyboard layouts (also glyph codes in the font)
#define KEY_ENTER 0x01
#define KEY_DEL   0x02

#define KB_ROWS     3
#define KB_MAX_KEYS 10

typedef struct {
    const char *name;                   // "FRANCAIS" / "ENGLISH"
    const char (*solutions)[WORD_LEN];  // upper-case, no terminator
    u16 n_solutions;
    const char (*valid)[WORD_LEN];      // sorted, upper-case, no terminator
    u16 n_valid;
    const u16 *challenge_seq;           // permutation of solution indices
    const char *kb_rows[KB_ROWS];       // key codes, row by row, NUL terminated

    // UI strings
    const char *press_start;
    const char *menu_language;
    const char *menu_classic;
    const char *menu_challenge;
    const char *menu_stats;
    const char *menu_help;
    const char *mode_classic;
    const char *mode_challenge;         // printf-style with the challenge number
    const char *msg_too_short;
    const char *msg_not_in_list;
    const char *win_msgs[MAX_GUESSES];  // by number of guesses used
    const char *lose_msg;               // followed by the target word
    const char *stats_title;
    const char *stats_played;
    const char *stats_win_rate;
    const char *stats_streak;
    const char *stats_max_streak;
    const char *stats_distribution;
    const char *stats_challenges;
    const char *stats_back;
    const char *result_prompt;          // "A: AGAIN  B: MENU"
    const char *challenge_resumed;
    const char *game_help;              // controls hint in game
} Language;

extern const Language languages[LANG_COUNT];

#endif
