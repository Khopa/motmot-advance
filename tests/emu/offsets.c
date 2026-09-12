// Prints the byte offsets the Lua scenarios need to read the game state
// from RAM (one "name=value" per line). Compiled and run by run.py with the
// host compiler, so the Lua side never hard-codes a struct layout.
#include <stddef.h>
#include <stdio.h>
#include "game_state.h"
#include "keyboard.h"
#include "stats.h"

#define P(prefix, type, field) printf(#prefix "." #field "=%zu\n", offsetof(type, field))

int main(void)
{
    P(game, GameState, lang);
    P(game, GameState, mode);
    P(game, GameState, status);
    P(game, GameState, target);
    P(game, GameState, n_guesses);
    P(game, GameState, guesses);
    P(game, GameState, feedback);
    P(game, GameState, cur_len);
    P(game, GameState, current);
    P(game, GameState, key_state);
    P(marathon, MarathonState, difficulty);
    P(marathon, MarathonState, hp);
    P(marathon, MarathonState, hp_max);
    P(marathon, MarathonState, score);
    P(marathon, MarathonState, new_record);
    P(kb, KbCursor, row);
    P(kb, KbCursor, col);
    P(save, SaveData, lang);
    P(save, SaveData, sound_on);
    P(save, SaveData, marathon_diff);
    P(save, SaveData, played);
    P(save, SaveData, won);
    P(save, SaveData, lost);
    P(save, SaveData, streak);
    P(save, SaveData, max_streak);
    P(save, SaveData, dist);
    P(save, SaveData, marathon_best);
    printf("sizeof.SaveData=%zu\n", sizeof(SaveData));
    return 0;
}
