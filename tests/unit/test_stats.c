// SRAM persistence: defaults, round trip, corruption, statistics updates.
#include "test.h"
#include "stats.h"

TEST(blank_sram_gives_defaults)
{
    stats_load();
    CHECK_EQ(save.played, 0);
    CHECK_EQ(save.won, 0);
    CHECK_EQ(save.streak, 0);
    CHECK_EQ(save.lang, LANG_FR);
    CHECK_EQ(save.sound_on, 1);
    CHECK_EQ(save.marathon_diff, DIFF_EASY);
    CHECK_EQ(save.marathon_best[0], 0);
    CHECK_EQ(save.ta_length, 0);
    CHECK_EQ(save.classic_won, 0);
    CHECK_MEM(save.initials, "AAA", 3);
    for (int l = 0; l < TA_LENGTH_COUNT; l++)
        for (int i = 0; i < TA_TOP; i++) CHECK_EQ(save.ta_board[l][i].used, 0);
    CHECK(save.rng_state != 0);
}

TEST(load_sets_sram_wait_state)
{
    stats_load();
    CHECK_EQ(REG_WAITCNT, WS_STANDARD | WS_SRAM_8);
}

TEST(defaults_are_written_back)
{
    stats_load();
    // the SRAM now holds a valid image: loading again must not change anything
    save.played = 0xBEEF;                       // dirty the RAM copy only
    stats_load();
    CHECK_EQ(save.played, 0);
    CHECK_EQ(save.lang, LANG_FR);
}

TEST(save_then_load_round_trip)
{
    stats_load();
    save.played = 12;
    save.won = 9;
    save.lost = 3;
    save.streak = 4;
    save.max_streak = 7;
    save.dist[2] = 5;
    save.lang = LANG_EN;
    save.sound_on = 0;
    save.marathon_diff = DIFF_HARD;
    save.marathon_best[1] = 21;
    save.rng_state = 0x12345678;
    save.ta_length = 2;
    save.classic_won = 8;
    memcpy(save.initials, "CLM", 3);
    save.ta_board[1][0] = (TimeRecord){ .frames = 5400, .initials = { 'A', 'B', 'C' }, .used = 1 };
    stats_save();
    memset(&save, 0, sizeof save);
    stats_load();
    CHECK_EQ(save.played, 12);
    CHECK_EQ(save.won, 9);
    CHECK_EQ(save.lost, 3);
    CHECK_EQ(save.streak, 4);
    CHECK_EQ(save.max_streak, 7);
    CHECK_EQ(save.dist[2], 5);
    CHECK_EQ(save.lang, LANG_EN);
    CHECK_EQ(save.sound_on, 0);
    CHECK_EQ(save.marathon_diff, DIFF_HARD);
    CHECK_EQ(save.marathon_best[1], 21);
    CHECK_EQ(save.rng_state, 0x12345678);
    CHECK_EQ(save.ta_length, 2);
    CHECK_EQ(save.classic_won, 8);
    CHECK_MEM(save.initials, "CLM", 3);
    CHECK_EQ(save.ta_board[1][0].frames, 5400);
    CHECK_MEM(save.ta_board[1][0].initials, "ABC", 3);
    CHECK_EQ(save.ta_board[1][1].used, 0);
}

TEST(sram_is_written_byte_for_byte)
{
    stats_load();
    save.played = 0x1234;
    stats_save();
    CHECK_EQ(memcmp(host_sram, &save, sizeof save), 0);
}

TEST(corrupted_byte_resets_to_defaults)
{
    stats_load();
    save.played = 50;
    stats_save();
    host_sram[offsetof(SaveData, played)] ^= 0x01;      // flip a data bit, checksum now wrong
    stats_load();
    CHECK_EQ(save.played, 0);
}

TEST(wrong_magic_resets_to_defaults)
{
    stats_load();
    save.played = 50;
    stats_save();
    host_sram[0] = 'X';
    stats_load();
    CHECK_EQ(save.played, 0);
}

TEST(wrong_version_resets_to_defaults)
{
    stats_load();
    save.played = 50;
    stats_save();
    host_sram[offsetof(SaveData, version)] += 1;
    stats_load();
    CHECK_EQ(save.played, 0);
}

TEST(out_of_range_language_resets_to_defaults)
{
    stats_load();
    save.lang = LANG_COUNT + 3;
    stats_save();                                        // checksum is valid...
    stats_load();                                        // ...but the value is not
    CHECK_EQ(save.lang, LANG_FR);
}

TEST(out_of_range_difficulty_resets_to_defaults)
{
    stats_load();
    save.marathon_diff = DIFF_COUNT;
    stats_save();
    stats_load();
    CHECK_EQ(save.marathon_diff, DIFF_EASY);
}

TEST(out_of_range_time_attack_length_resets_to_defaults)
{
    stats_load();
    save.ta_length = TA_LENGTH_COUNT;
    stats_save();
    stats_load();
    CHECK_EQ(save.ta_length, 0);
}

TEST(all_zero_sram_is_not_valid)
{
    memset(host_sram, 0, sizeof host_sram);
    stats_load();
    CHECK_EQ(save.played, 0);
    CHECK_EQ(save.sound_on, 1);
}

TEST(record_win_updates_streak_and_distribution)
{
    stats_load();
    stats_record_result(true, 3);
    CHECK_EQ(save.played, 1);
    CHECK_EQ(save.won, 1);
    CHECK_EQ(save.lost, 0);
    CHECK_EQ(save.streak, 1);
    CHECK_EQ(save.max_streak, 1);
    CHECK_EQ(save.dist[2], 1);
    stats_record_result(true, 1);
    stats_record_result(true, 6);
    CHECK_EQ(save.streak, 3);
    CHECK_EQ(save.max_streak, 3);
    CHECK_EQ(save.dist[0], 1);
    CHECK_EQ(save.dist[5], 1);
}

TEST(record_loss_resets_streak_but_keeps_best)
{
    stats_load();
    stats_record_result(true, 2);
    stats_record_result(true, 2);
    stats_record_result(false, 6);
    CHECK_EQ(save.played, 3);
    CHECK_EQ(save.lost, 1);
    CHECK_EQ(save.streak, 0);
    CHECK_EQ(save.max_streak, 2);
    stats_record_result(true, 4);
    CHECK_EQ(save.streak, 1);
    CHECK_EQ(save.max_streak, 2);
}

TEST(record_ignores_invalid_guess_counts)
{
    stats_load();
    stats_record_result(true, 0);
    stats_record_result(true, 7);
    unsigned total = 0;
    for (int i = 0; i < MAX_GUESSES; i++) total += save.dist[i];
    CHECK_EQ(total, 0);
    CHECK_EQ(save.won, 2);
}

TEST(recent_words_ring_buffer)
{
    stats_load();
    CHECK(!stats_recently_played(LANG_FR, 0));        // index 0 must not read as "played"
    for (u16 i = 0; i < RECENT_WORDS; i++) stats_push_recent(LANG_FR, i);
    for (u16 i = 0; i < RECENT_WORDS; i++) CHECK(stats_recently_played(LANG_FR, i));
    CHECK(!stats_recently_played(LANG_FR, RECENT_WORDS));
    stats_push_recent(LANG_FR, 100);                   // evicts the oldest (0)
    CHECK(!stats_recently_played(LANG_FR, 0));
    CHECK(stats_recently_played(LANG_FR, 1));
    CHECK(stats_recently_played(LANG_FR, 100));
}

TEST(recent_words_are_per_language)
{
    stats_load();
    stats_push_recent(LANG_FR, 5);
    CHECK(stats_recently_played(LANG_FR, 5));
    CHECK(!stats_recently_played(LANG_EN, 5));
}

static const TestCase stats_tests[] = {
    T(blank_sram_gives_defaults), T(load_sets_sram_wait_state), T(defaults_are_written_back),
    T(save_then_load_round_trip), T(sram_is_written_byte_for_byte), T(corrupted_byte_resets_to_defaults),
    T(wrong_magic_resets_to_defaults), T(wrong_version_resets_to_defaults),
    T(out_of_range_language_resets_to_defaults), T(out_of_range_difficulty_resets_to_defaults),
    T(out_of_range_time_attack_length_resets_to_defaults),
    T(all_zero_sram_is_not_valid),
    T(record_win_updates_streak_and_distribution), T(record_loss_resets_streak_but_keeps_best),
    T(record_ignores_invalid_guess_counts), T(recent_words_ring_buffer), T(recent_words_are_per_language),
};
SUITE(stats)
