// Platform-independent game rules: score a guess, validate, type a row.
#include <string.h>
#include "logic.h"

void logic_score(const char guess[WORD_LEN], const char target[WORD_LEN], u8 out[WORD_LEN])
{
    u8 remaining[26];   // letters of the target not yet matched
    memset(remaining, 0, sizeof remaining);

    // pass 1: exact matches
    for (int i = 0; i < WORD_LEN; i++) {
        if (guess[i] == target[i]) {
            out[i] = FB_CORRECT;
        } else {
            out[i] = FB_ABSENT;
            remaining[target[i] - 'A']++;
        }
    }
    // pass 2: misplaced letters, each target letter can be claimed once
    for (int i = 0; i < WORD_LEN; i++) {
        if (out[i] == FB_CORRECT) continue;
        u8 *r = &remaining[guess[i] - 'A'];
        if (*r > 0) {
            out[i] = FB_PRESENT;
            (*r)--;
        }
    }
}

bool logic_word_in_list(const char (*list)[WORD_LEN], int count, const char word[WORD_LEN])
{
    int lo = 0, hi = count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        int c = memcmp(list[mid], word, WORD_LEN);
        if (c == 0) return true;
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return false;
}

bool logic_is_valid_guess(const Language *lang, const char word[WORD_LEN])
{
    return logic_word_in_list(lang->valid, lang->n_valid, word);
}

void game_init(GameState *g, u8 lang, u8 mode, const char target[WORD_LEN])
{
    memset(g, 0, sizeof *g);
    g->lang = lang;
    g->mode = mode;
    g->status = STATUS_PLAYING;
    memcpy(g->target, target, WORD_LEN);
}

bool game_type_letter(GameState *g, char letter)
{
    if (g->status != STATUS_PLAYING || g->cur_len >= WORD_LEN) return false;
    if (letter < 'A' || letter > 'Z') return false;
    g->current[g->cur_len++] = letter;
    return true;
}

bool game_backspace(GameState *g)
{
    if (g->status != STATUS_PLAYING || g->cur_len == 0) return false;
    g->cur_len--;
    g->current[g->cur_len] = 0;
    return true;
}

// `guess` may alias g->current, so read everything before clearing it.
static void commit_row(GameState *g, const char guess[WORD_LEN])
{
    int row = g->n_guesses;
    memcpy(g->guesses[row], guess, WORD_LEN);
    logic_score(g->guesses[row], g->target, g->feedback[row]);

    // keyboard colours only ever improve: absent < present < correct
    for (int i = 0; i < WORD_LEN; i++) {
        u8 *k = &g->key_state[g->guesses[row][i] - 'A'];
        if (g->feedback[row][i] > *k) *k = g->feedback[row][i];
    }
    g->n_guesses++;
    g->cur_len = 0;
    memset(g->current, 0, sizeof g->current);

    if (memcmp(g->guesses[row], g->target, WORD_LEN) == 0)
        g->status = STATUS_WON;
    else if (g->n_guesses >= MAX_GUESSES)
        g->status = STATUS_LOST;
}

SubmitResult game_submit(GameState *g, const Language *lang)
{
    if (g->status != STATUS_PLAYING) return SUBMIT_TOO_SHORT;
    if (g->cur_len < WORD_LEN) return SUBMIT_TOO_SHORT;
    if (!logic_is_valid_guess(lang, g->current)) return SUBMIT_NOT_IN_LIST;

    commit_row(g, g->current);
    if (g->status == STATUS_WON)  return SUBMIT_WON;
    if (g->status == STATUS_LOST) return SUBMIT_LOST;
    return SUBMIT_OK;
}

void game_replay_guess(GameState *g, const char guess[WORD_LEN])
{
    if (g->status == STATUS_PLAYING && g->n_guesses < MAX_GUESSES)
        commit_row(g, guess);
}
