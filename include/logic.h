// Platform-independent game rules (no hardware access; host-testable).
#ifndef LOGIC_H
#define LOGIC_H

#include "common.h"
#include "game_state.h"
#include "lang.h"

typedef enum {
    SUBMIT_OK = 0,      // row accepted, game continues
    SUBMIT_WON,
    SUBMIT_LOST,
    SUBMIT_TOO_SHORT,
    SUBMIT_NOT_IN_LIST,
} SubmitResult;

// Colour a guess against the target (repeated letters are matched at most once).
void logic_score(const char guess[WORD_LEN], const char target[WORD_LEN], u8 out[WORD_LEN]);

// Binary search in a sorted list of 5-letter upper-case words.
bool logic_word_in_list(const char (*list)[WORD_LEN], int count, const char word[WORD_LEN]);
bool logic_is_valid_guess(const Language *lang, const char word[WORD_LEN]);

void game_init(GameState *g, u8 lang, u8 mode, const char target[WORD_LEN]);
bool game_type_letter(GameState *g, char letter);   // 'A'..'Z'
bool game_backspace(GameState *g);
// Commit the current row; on success feedback/key_state/status are updated.
SubmitResult game_submit(GameState *g, const Language *lang);
// Re-apply a committed guess (used when resuming a saved Challenge).
void game_replay_guess(GameState *g, const char guess[WORD_LEN]);

#endif
