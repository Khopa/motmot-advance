// Sound effects on the GBA's Game Boy tone generators (square 1, square 2,
// noise). No samples, no mixer: a tiny frame-driven sequencer in plain C.
#ifndef SOUND_H
#define SOUND_H

#include "common.h"

typedef enum {
    SFX_KEY,        // letter typed
    SFX_DELETE,     // letter erased
    SFX_MOVE,       // menu cursor moved
    SFX_SELECT,     // menu item chosen
    SFX_ERROR,      // unknown word / too short: buzzer
    SFX_ABSENT,     // reveal: grey letter
    SFX_PRESENT,    // reveal: yellow letter
    SFX_CORRECT,    // reveal: green letter
    SFX_WIN,        // fanfare
    SFX_LOSE,       // sad descending notes
    SFX_COUNT
} SfxId;

void sound_init(void);
void sound_set_enabled(bool on);
bool sound_enabled(void);
void sfx_play(SfxId id);
void sound_update(void);        // once per frame

#endif
