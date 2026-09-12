// PSG sound effects. Each effect is up to three tracks (square 1, square 2,
// noise) of steps; every step programs the channel once and lasts a number
// of frames. Tracks end with a step of 0 frames.
#include "sound.h"

// --- notes -----------------------------------------------------------------
// Period values for octave 4 (C4 = 262 Hz); the tone generator plays
// 131072 / (2048 - rate) Hz, so a higher octave halves the divider.
static const u16 base_rates[12] = {
    8013, 7566, 7144, 6742, 6362, 6005, 5666, 5346, 5048, 4766, 4499, 4246,
};
enum { C = 0, CS, D, DS, E, F, FS, G, GS, A, AS, B, REST = 0xFF };

static u16 note_rate(u8 note, s8 oct)
{
    return 2048 - (base_rates[note] >> (4 + oct));
}

// --- step tables -----------------------------------------------------------

typedef struct {
    u8 note;        // C..B or REST
    s8 oct;         // 0 = 4th octave (C4), 1 = C5, 2 = C6
    u8 frames;      // 0 terminates the track
    u8 vol;         // 0-15 initial volume
    u8 decay;       // envelope step (0 = hold, 1 = fast fade ... 7 = slow)
    u8 duty;        // 0 = 12.5%, 1 = 25%, 2 = 50%, 3 = 75%
} Step;

typedef struct {
    u8 shift, ratio;   // noise clock: 524288 / ratio / 2^(shift+1)
    u8 frames;
    u8 vol, decay;
} NoiseStep;

typedef struct {
    const Step *sq1;
    const Step *sq2;
    const NoiseStep *noise;
} Sfx;

#define END_STEP  { REST, 0, 0, 0, 0, 0 }
#define END_NOISE { 0, 0, 0, 0, 0 }

static const Step key_sq1[]    = { { E, 2, 2, 8, 1, 2 }, END_STEP };
static const Step delete_sq1[] = { { C, 1, 2, 8, 1, 2 }, END_STEP };
static const Step move_sq1[]   = { { A, 1, 1, 5, 1, 1 }, END_STEP };
static const Step select_sq1[] = { { C, 2, 3, 9, 1, 2 }, { G, 2, 4, 9, 2, 2 }, END_STEP };

// buzzer: two harsh low bursts with a touch of noise
static const Step error_sq1[]  = { { B, -1, 8, 12, 0, 0 }, { REST, 0, 4, 0, 0, 0 },
                                   { B, -1, 8, 12, 2, 0 }, END_STEP };
static const NoiseStep error_noise[] = { { 5, 3, 8, 6, 1 }, { 0, 0, 4, 0, 0 },
                                         { 5, 3, 8, 6, 2 }, END_NOISE };

// reveal: dull "tuk" / medium "bip" / bright "ding"
static const Step absent_sq1[]  = { { G, -1, 3, 6, 1, 1 }, END_STEP };
static const Step present_sq1[] = { { A, 1, 4, 10, 2, 2 }, END_STEP };
static const Step correct_sq1[] = { { E, 2, 6, 12, 3, 2 }, END_STEP };
static const Step correct_sq2[] = { { E, 3, 6, 6, 3, 2 }, END_STEP };

// win: rising fanfare with a harmony a third above
static const Step win_sq1[] = { { C, 1, 6, 12, 0, 2 }, { E, 1, 6, 12, 0, 2 },
                                { G, 1, 6, 12, 0, 2 }, { C, 2, 24, 12, 5, 2 }, END_STEP };
static const Step win_sq2[] = { { REST, 0, 3, 0, 0, 0 }, { E, 1, 6, 8, 0, 2 }, { G, 1, 6, 8, 0, 2 },
                                { C, 2, 6, 8, 0, 2 }, { E, 2, 24, 8, 5, 2 }, END_STEP };

// lose: three descending steps and a long low note
static const Step lose_sq1[] = { { E, 0, 8, 10, 0, 2 }, { DS, 0, 8, 10, 0, 2 },
                                 { D, 0, 8, 10, 0, 2 }, { CS, 0, 30, 10, 6, 2 }, END_STEP };
static const Step lose_sq2[] = { { REST, 0, 24, 0, 0, 0 }, { A, -1, 30, 6, 6, 2 }, END_STEP };

// hurt: two quick falling notes
static const Step hurt_sq1[] = { { A, 0, 4, 11, 0, 1 }, { E, 0, 8, 11, 2, 1 }, END_STEP };

static const Sfx sfx_table[SFX_COUNT] = {
    [SFX_KEY]     = { key_sq1, 0, 0 },
    [SFX_DELETE]  = { delete_sq1, 0, 0 },
    [SFX_MOVE]    = { move_sq1, 0, 0 },
    [SFX_SELECT]  = { select_sq1, 0, 0 },
    [SFX_ERROR]   = { error_sq1, 0, error_noise },
    [SFX_ABSENT]  = { absent_sq1, 0, 0 },
    [SFX_PRESENT] = { present_sq1, 0, 0 },
    [SFX_CORRECT] = { correct_sq1, correct_sq2, 0 },
    [SFX_WIN]     = { win_sq1, win_sq2, 0 },
    [SFX_LOSE]    = { lose_sq1, lose_sq2, 0 },
    [SFX_HURT]    = { hurt_sq1, 0, 0 },
};

// --- sequencer -------------------------------------------------------------

typedef struct {
    const Step *steps;          // NULL when idle
    const NoiseStep *nsteps;
    u8 index;
    u8 remaining;
} Track;

static Track tracks[3];         // 0 = square 1, 1 = square 2, 2 = noise
static bool enabled = true;

static void square_set(int ch, const Step *s)
{
    u16 cnt = SSQR_ENV_BUILD(s->vol, 0, s->decay) | ((s->duty & 3) << 6);
    u16 freq = SFREQ_RESET | note_rate(s->note, s->oct);
    if (s->note == REST) {
        cnt = 0;
        freq = SFREQ_RESET;
    }
    if (ch == 0) {
        REG_SND1SWEEP = SSW_OFF;
        REG_SND1CNT = cnt;
        REG_SND1FREQ = freq;
    } else {
        REG_SND2CNT = cnt;
        REG_SND2FREQ = freq;
    }
}

static void noise_set(const NoiseStep *s)
{
    REG_SND4CNT = SSQR_ENV_BUILD(s->vol, 0, s->decay);
    REG_SND4FREQ = SFREQ_RESET | (s->shift << 4) | (s->ratio & 7);
}

static void square_silence(int ch)
{
    if (ch == 0) { REG_SND1CNT = 0; REG_SND1FREQ = SFREQ_RESET; }
    else         { REG_SND2CNT = 0; REG_SND2FREQ = SFREQ_RESET; }
}

void sound_init(void)
{
    REG_SNDSTAT = SSTAT_ENABLE;
    REG_SNDDMGCNT = SDMG_BUILD_LR(SDMG_SQR1 | SDMG_SQR2 | SDMG_NOISE, 7);
    REG_SNDDSCNT = SDS_DMG100;
    for (int i = 0; i < 3; i++) tracks[i].steps = 0, tracks[i].nsteps = 0;
    square_silence(0);
    square_silence(1);
    REG_SND4CNT = 0;
}

void sound_set_enabled(bool on)
{
    enabled = on;
    if (!on) {
        for (int i = 0; i < 3; i++) tracks[i].steps = 0, tracks[i].nsteps = 0;
        square_silence(0);
        square_silence(1);
        REG_SND4CNT = 0;
        REG_SND4FREQ = SFREQ_RESET;
    }
}

bool sound_enabled(void)
{
    return enabled;
}

static void track_start(Track *t, const Step *steps, const NoiseStep *nsteps)
{
    t->steps = steps;
    t->nsteps = nsteps;
    t->index = 0;
    t->remaining = 0;       // first step is programmed by the next update
}

void sfx_play(SfxId id)
{
    if (!enabled || id >= SFX_COUNT) return;
    const Sfx *s = &sfx_table[id];
    // a new effect replaces whatever was playing on the tracks it uses
    if (s->sq1)   track_start(&tracks[0], s->sq1, 0);
    if (s->sq2)   track_start(&tracks[1], s->sq2, 0);
    if (s->noise) track_start(&tracks[2], 0, s->noise);
    sound_update();
}

void sound_update(void)
{
    for (int ch = 0; ch < 2; ch++) {
        Track *t = &tracks[ch];
        if (!t->steps) continue;
        if (t->remaining > 0) { t->remaining--; continue; }
        const Step *s = &t->steps[t->index++];
        if (s->frames == 0) {
            t->steps = 0;
            square_silence(ch);
            continue;
        }
        square_set(ch, s);
        t->remaining = s->frames - 1;
    }

    Track *t = &tracks[2];
    if (t->nsteps) {
        if (t->remaining > 0) {
            t->remaining--;
        } else {
            const NoiseStep *s = &t->nsteps[t->index++];
            if (s->frames == 0) {
                t->nsteps = 0;
                REG_SND4CNT = 0;
                REG_SND4FREQ = SFREQ_RESET;
            } else {
                if (s->vol) noise_set(s);
                else { REG_SND4CNT = 0; REG_SND4FREQ = SFREQ_RESET; }
                t->remaining = s->frames - 1;
            }
        }
    }
}
