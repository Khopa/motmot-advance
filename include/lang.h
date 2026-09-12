// Language table: word lists, keyboard layout and UI strings per language.
#ifndef LANG_H
#define LANG_H

#include "common.h"
#include "game_state.h"

typedef enum { LANG_FR = 0, LANG_EN, LANG_ES, LANG_DE, LANG_IT, LANG_COUNT } LangId;

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
    const char *kb_rows[KB_ROWS];       // key codes, row by row, NUL terminated

    // --- UI strings (font character set only: A-Z 0-9 ! ? : . - / % > < # ' ,)
    const char *press_start;
    // main menu
    const char *menu_classic;
    const char *menu_marathon;
    const char *menu_time_attack;
    const char *menu_records;
    const char *menu_stats;
    const char *menu_options;
    const char *menu_help;              // "A: OK  B: BACK"
    // options
    const char *options_title;
    const char *opt_language;
    const char *opt_sound;
    const char *on;
    const char *off;
    // mode selection
    const char *difficulty[DIFF_COUNT];
    const char *words;                  // "WORDS" (after a number)
    const char *best;                   // "BEST"
    const char *select_help;            // "A: START  B: BACK"
    // in game
    const char *mode_classic;
    const char *score;                  // Marathon status line
    const char *word;                   // Time Attack status line: "WORD 3/10"
    const char *msg_too_short;
    const char *msg_not_in_list;
    const char *win_msgs[MAX_GUESSES];  // by number of guesses used
    const char *lose_msg;               // followed by the target word
    const char *game_help;
    const char *quit_question;          // "QUIT?"          (modal box, line 1)
    const char *quit_choices;           // "A: YES   B: NO" (modal box, line 2)
    // results
    const char *result_prompt;          // "A: PLAY AGAIN  B: MENU"
    const char *marathon_over;
    const char *new_record;
    const char *ta_done;                // "TIME ATTACK COMPLETE"
    const char *time;                   // "TIME"
    const char *penalties;              // "PENALTIES"
    const char *enter_initials;
    const char *initials_help;          // "UP/DOWN: LETTER  A: NEXT"
    // records
    const char *records_title;
    const char *records_help;           // "</>: PAGE  B: BACK"
    // statistics
    const char *stats_title;
    const char *stats_played;           // words played, all modes
    const char *stats_win_rate;
    const char *stats_streak;
    const char *stats_max_streak;
    const char *stats_distribution;
    const char *stats_classic;          // classic games solved
    const char *stats_back;
} Language;

extern const Language languages[LANG_COUNT];

#endif
