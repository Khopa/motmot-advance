// Persistent data (SRAM): statistics, Challenge progress, RNG state.
#ifndef STATS_H
#define STATS_H

#include "common.h"
#include "lang.h"

#define RECENT_WORDS 8      // classic-mode words kept to avoid near repeats

typedef struct {
    u32  magic;
    u8   version;
    u8   lang;                              // last language chosen in the menu
    u8   sound_on;                          // sound effects toggle
    u16  played;
    u16  won;
    u16  lost;
    u16  streak;
    u16  max_streak;
    u16  dist[MAX_GUESSES];                 // wins by number of guesses
    u16  challenge_done[LANG_COUNT];        // completed challenges per language
    u16  challenge_won;
    u32  rng_state;                         // carried across power cycles
    // Challenge in progress (at most one at a time)
    u8   ch_active;
    u8   ch_lang;
    u8   ch_n_guesses;
    char ch_guesses[MAX_GUESSES][WORD_LEN];
    // Recently played classic words (solution indices) per language
    u16  recent[LANG_COUNT][RECENT_WORDS];
    u8   recent_pos[LANG_COUNT];
    u16  checksum;
} SaveData;

extern SaveData save;       // RAM copy; stats_save() commits it to SRAM

void stats_load(void);      // read SRAM (or initialise defaults if blank)
void stats_save(void);
void stats_record_result(bool won, int n_guesses);
bool stats_recently_played(u8 lang, u16 solution_index);
void stats_push_recent(u8 lang, u16 solution_index);

#endif
