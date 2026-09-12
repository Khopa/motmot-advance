// PSG sound effects: what the sequencer writes to the tone-generator registers.
#include "test.h"
#include "sound.h"

#define ENV_VOL(reg) ((reg) >> 12)
#define RATE(reg)    ((reg) & 0x7FF)

// libtonc formula: 2048 - (rate_of_octave_4 >> (4 + oct))
static u16 rate(u16 base, int oct) { return 2048 - (base >> (4 + oct)); }
#define RATE_E 6362
#define RATE_B 4246

TEST(init_enables_all_three_channels)
{
    sound_init();
    CHECK(REG_SNDSTAT & SSTAT_ENABLE);
    CHECK_EQ(REG_SNDDMGCNT, SDMG_BUILD_LR(SDMG_SQR1 | SDMG_SQR2 | SDMG_NOISE, 7));
    CHECK_EQ(REG_SNDDSCNT, SDS_DMG100);
    CHECK(sound_enabled());
}

TEST(key_click_plays_on_square_1)
{
    sound_init();
    sfx_play(SFX_KEY);
    CHECK_EQ(ENV_VOL(REG_SND1CNT), 8);
    CHECK(REG_SND1FREQ & SFREQ_RESET);
    CHECK_EQ(RATE(REG_SND1FREQ), rate(RATE_E, 2));      // E6
    CHECK_EQ(REG_SND1SWEEP, SSW_OFF);
    CHECK_EQ(REG_SND2CNT, 0);                            // other channels untouched
    CHECK_EQ(REG_SND4CNT, 0);
}

TEST(step_lasts_its_frames_then_silence)
{
    sound_init();
    sfx_play(SFX_KEY);                                   // 2-frame step
    REG_SND1FREQ = 0;                                    // sentinel: detect further writes
    sound_update();                                      // frame 2 of the step: no write
    CHECK_EQ(REG_SND1FREQ, 0);
    CHECK_EQ(ENV_VOL(REG_SND1CNT), 8);
    sound_update();                                      // end of track: channel silenced
    CHECK_EQ(REG_SND1CNT, 0);
    CHECK_EQ(REG_SND1FREQ, SFREQ_RESET);
    REG_SND1FREQ = 0;
    sound_update();                                      // idle: nothing written
    CHECK_EQ(REG_SND1FREQ, 0);
}

TEST(buzzer_uses_square_1_and_noise)
{
    sound_init();
    sfx_play(SFX_ERROR);
    CHECK_EQ(ENV_VOL(REG_SND1CNT), 12);
    CHECK_EQ(RATE(REG_SND1FREQ), rate(RATE_B, -1));     // B3
    CHECK_EQ(ENV_VOL(REG_SND4CNT), 6);
    CHECK(REG_SND4FREQ & SFREQ_RESET);
    // 8 frames of buzz, 4 of rest, 8 of buzz
    for (int i = 0; i < 7; i++) sound_update();
    CHECK_EQ(ENV_VOL(REG_SND1CNT), 12);
    sound_update();                                      // rest step
    CHECK_EQ(REG_SND1CNT, 0);
    CHECK_EQ(REG_SND4CNT, 0);
    for (int i = 0; i < 4; i++) sound_update();          // second burst
    CHECK_EQ(ENV_VOL(REG_SND1CNT), 12);
    CHECK_EQ(ENV_VOL(REG_SND4CNT), 6);
}

TEST(chord_effects_drive_both_squares)
{
    sound_init();
    sfx_play(SFX_CORRECT);
    CHECK_EQ(ENV_VOL(REG_SND1CNT), 12);
    CHECK_EQ(ENV_VOL(REG_SND2CNT), 6);
    CHECK_EQ(RATE(REG_SND2FREQ), rate(RATE_E, 3));      // an octave above square 1
    CHECK_EQ(RATE(REG_SND1FREQ), rate(RATE_E, 2));
}

TEST(rest_steps_silence_the_channel)
{
    sound_init();
    sfx_play(SFX_WIN);                                   // square 2 starts with a 3-frame rest
    CHECK_EQ(REG_SND2CNT, 0);
    CHECK_EQ(ENV_VOL(REG_SND1CNT), 12);
    for (int i = 0; i < 3; i++) sound_update();
    CHECK_EQ(ENV_VOL(REG_SND2CNT), 8);                   // harmony comes in
}

TEST(new_effect_replaces_the_running_one)
{
    sound_init();
    sfx_play(SFX_WIN);
    sound_update();
    sfx_play(SFX_KEY);                                   // takes over square 1 only
    CHECK_EQ(ENV_VOL(REG_SND1CNT), 8);
    for (int i = 0; i < 2; i++) sound_update();
    CHECK_EQ(REG_SND1CNT, 0);                            // key click finished...
    for (int i = 0; i < 2; i++) sound_update();
    CHECK_EQ(ENV_VOL(REG_SND2CNT), 8);                   // ...while the win harmony carries on
}

TEST(every_effect_terminates)
{
    for (int id = 0; id < SFX_COUNT; id++) {
        sound_init();
        sfx_play((SfxId)id);
        int frames = 0;
        bool silent = false;
        while (frames < 200 && !silent) {
            sound_update();
            frames++;
            silent = REG_SND1CNT == 0 && REG_SND2CNT == 0 && REG_SND4CNT == 0;
        }
        CHECK(silent);
        CHECK(frames < 120);                             // no effect longer than 2 s
    }
}

TEST(disabled_sound_writes_nothing)
{
    sound_init();
    sound_set_enabled(false);
    CHECK(!sound_enabled());
    REG_SND1CNT = REG_SND1FREQ = REG_SND2CNT = REG_SND4CNT = 0x5555;   // sentinels
    sfx_play(SFX_KEY);
    sfx_play(SFX_ERROR);
    sfx_play(SFX_WIN);
    for (int i = 0; i < 10; i++) sound_update();
    CHECK_EQ(REG_SND1CNT, 0x5555);
    CHECK_EQ(REG_SND1FREQ, 0x5555);
    CHECK_EQ(REG_SND2CNT, 0x5555);
    CHECK_EQ(REG_SND4CNT, 0x5555);
}

TEST(disabling_cuts_a_running_effect)
{
    sound_init();
    sfx_play(SFX_WIN);
    sound_set_enabled(false);
    CHECK_EQ(REG_SND1CNT, 0);
    CHECK_EQ(REG_SND2CNT, 0);
    CHECK_EQ(REG_SND4CNT, 0);
    sound_set_enabled(true);
    sfx_play(SFX_KEY);
    CHECK_EQ(ENV_VOL(REG_SND1CNT), 8);
}

TEST(invalid_effect_id_is_ignored)
{
    sound_init();
    REG_SND1CNT = 0x5555;
    sfx_play((SfxId)SFX_COUNT);
    sfx_play((SfxId)200);
    CHECK_EQ(REG_SND1CNT, 0x5555);
}

static const TestCase sound_tests[] = {
    T(init_enables_all_three_channels), T(key_click_plays_on_square_1), T(step_lasts_its_frames_then_silence),
    T(buzzer_uses_square_1_and_noise), T(chord_effects_drive_both_squares), T(rest_steps_silence_the_channel),
    T(new_effect_replaces_the_running_one), T(every_effect_terminates), T(disabled_sound_writes_nothing),
    T(disabling_cuts_a_running_effect), T(invalid_effect_id_is_ignored),
};
SUITE(sound)
