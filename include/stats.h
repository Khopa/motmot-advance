// Persistent data (SRAM): statistics, options, RNG state.
#ifndef STATS_H
#define STATS_H

#include "common.h"
#include "lang.h"
#include "game_state.h"
#include "time_attack.h"

#define RECENT_WORDS 8      // classic-mode words kept to avoid near repeats

typedef struct {
    u32  magic;
    u8   version;
    u8   lang;                              // last language chosen in the menu
    u8   sound_on;                          // sound effects toggle
    u8   marathon_diff;                     // last Marathon difficulty chosen
    u16  played;
    u16  won;
    u16  lost;
    u16  streak;
    u16  max_streak;
    u16  dist[MAX_GUESSES];                 // wins by number of guesses
    u32  rng_state;                         // carried across power cycles
    // Recently played classic words (solution indices) per language
    u16  recent[LANG_COUNT][RECENT_WORDS];
    u8   recent_pos[LANG_COUNT];
    u16  marathon_best[DIFF_COUNT];         // Marathon high score per difficulty
    TimeRecord ta_board[TA_LENGTH_COUNT][TA_TOP];   // Time Attack leaderboards
    char initials[3];                       // last initials entered
    u8   ta_length;                         // last Time Attack length chosen
    u16  checksum;
} SaveData;

extern SaveData save;       // RAM copy; stats_save() commits it to SRAM

void stats_load(void);      // read SRAM (or initialise defaults if blank)
void stats_save(void);
void stats_record_result(bool won, int n_guesses);
bool stats_recently_played(u8 lang, u16 solution_index);
void stats_push_recent(u8 lang, u16 solution_index);

#endif
