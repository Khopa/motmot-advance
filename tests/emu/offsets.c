// Prints the byte offsets the Lua scenarios need to read the game state
// from RAM (one "name=value" per line). Compiled and run by run.py with the
// host compiler, so the Lua side never hard-codes a struct layout.
#include <stddef.h>
#include <stdio.h>
#include "game_state.h"
#include "keyboard.h"
#include "stats.h"
#include "time_attack.h"

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
    P(save, SaveData, classic_won);
    P(save, SaveData, marathon_best);
    P(save, SaveData, ta_board);
    P(save, SaveData, initials);
    P(save, SaveData, ta_length);
    P(ta, TimeAttackState, length_idx);
    P(ta, TimeAttackState, total);
    P(ta, TimeAttackState, done);
    P(ta, TimeAttackState, missed);
    P(ta, TimeAttackState, frames);
    P(ta, TimeAttackState, running);
    P(ta, TimeAttackState, rank);
    P(record, TimeRecord, frames);
    P(record, TimeRecord, initials);
    P(record, TimeRecord, used);
    printf("sizeof.TimeRecord=%zu\n", sizeof(TimeRecord));
    printf("sizeof.SaveData=%zu\n", sizeof(SaveData));
    return 0;
}
