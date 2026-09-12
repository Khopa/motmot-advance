// Game rules: scoring, validation, typing a row, win / loss, keyboard colours.
#include "test.h"
#include "logic.h"

// tiny sorted list acting as a language
static const char words[][WORD_LEN] __attribute__((nonstring)) = {
    "ABBEY", "ALLEY", "APPLE", "CRANE", "EERIE", "LEVEL", "SPEED", "STEEL", "TESTS",
};
static const Language lang = {
    .name = "TEST", .solutions = words, .n_solutions = 3,
    .valid = words, .n_valid = sizeof words / sizeof words[0],
};

// score guess against target and render it as G (green) Y (yellow) . (grey)
static const char *score(const char *guess, const char *target)
{
    static char out[WORD_LEN + 1];
    u8 fb[WORD_LEN];
    logic_score(guess, target, fb);
    for (int i = 0; i < WORD_LEN; i++)
        out[i] = fb[i] == FB_CORRECT ? 'G' : fb[i] == FB_PRESENT ? 'Y' : '.';
    out[WORD_LEN] = 0;
    return out;
}

#define CHECK_SCORE(guess, target, expect) CHECK_MEM(score(guess, target), expect, WORD_LEN)

static void type(GameState *g, const char *w)
{
    for (; *w; w++) game_type_letter(g, *w);
}

// --- scoring ----------------------------------------------------------------

TEST(score_exact_match)        { CHECK_SCORE("CRANE", "CRANE", "GGGGG"); }
TEST(score_no_common_letter)   { CHECK_SCORE("FUZZY", "CRANE", "....."); }
TEST(score_single_present)     { CHECK_SCORE("CRANE", "SPEED", "....Y"); }
TEST(score_present_and_correct){ CHECK_SCORE("TESTS", "STEEL", "YYY.."); }
TEST(score_anagram)            { CHECK_SCORE("LEAST", "STEAL", "YYYYY"); }
TEST(score_all_shifted)        { CHECK_SCORE("ABCDE", "EABCD", "YYYYY"); }

// repeated letters: a target letter is claimed at most once, exact matches first
TEST(score_double_guess_single_target)   { CHECK_SCORE("SPEED", "ABBEY", "...G."); }
TEST(score_double_guess_both_present)    { CHECK_SCORE("EERIE", "SPEED", "YY..."); }
TEST(score_exact_wins_over_present)      { CHECK_SCORE("LEVEL", "STEEL", ".Y.GG"); }
TEST(score_two_present_one_exact)        { CHECK_SCORE("ALLEY", "LEVEL", ".YYG."); }
TEST(score_five_same_letters)            { CHECK_SCORE("AAAAA", "ABBEY", "G...."); }
TEST(score_triple_letter_target)         { CHECK_SCORE("EEEEE", "EERIE", "GG..G"); }
TEST(score_guess_more_copies_than_target){ CHECK_SCORE("LLLAA", "ALLEY", ".GGY."); }

// property: greens + yellows for a letter never exceed its count in the target
TEST(score_never_overclaims)
{
    static const char *pool[] = { "ABBEY", "ALLEY", "APPLE", "CRANE", "EERIE", "LEVEL",
                                  "SPEED", "STEEL", "TESTS", "AAAAA", "ABABA", "BAAAB" };
    int n = sizeof pool / sizeof pool[0];
    for (int a = 0; a < n; a++) {
        for (int b = 0; b < n; b++) {
            u8 fb[WORD_LEN];
            logic_score(pool[a], pool[b], fb);
            for (char c = 'A'; c <= 'Z'; c++) {
                int in_target = 0, claimed = 0;
                for (int i = 0; i < WORD_LEN; i++) {
                    if (pool[b][i] == c) in_target++;
                    if (pool[a][i] == c && fb[i] != FB_ABSENT) claimed++;
                }
                CHECK(claimed <= in_target);
            }
            for (int i = 0; i < WORD_LEN; i++)
                CHECK_EQ(fb[i] == FB_CORRECT, pool[a][i] == pool[b][i]);
        }
    }
}

// --- validation -------------------------------------------------------------

TEST(valid_first_and_last_entries)
{
    CHECK(logic_is_valid_guess(&lang, "ABBEY"));
    CHECK(logic_is_valid_guess(&lang, "TESTS"));
}
TEST(valid_middle_entry)      { CHECK(logic_is_valid_guess(&lang, "CRANE")); }
TEST(valid_rejects_unknown)   { CHECK(!logic_is_valid_guess(&lang, "ZZZZZ")); CHECK(!logic_is_valid_guess(&lang, "AAAAA")); }
TEST(valid_rejects_near_miss) { CHECK(!logic_is_valid_guess(&lang, "CRANK")); CHECK(!logic_is_valid_guess(&lang, "ABBEX")); }
TEST(valid_empty_list)        { CHECK(!logic_word_in_list(words, 0, "ABBEY")); }
TEST(valid_single_entry_list) { CHECK(logic_word_in_list(words, 1, "ABBEY")); CHECK(!logic_word_in_list(words, 1, "ALLEY")); }

// --- typing a row -----------------------------------------------------------

TEST(init_resets_everything)
{
    GameState g;
    memset(&g, 0xAA, sizeof g);
    game_init(&g, 1, MODE_MARATHON, "CRANE");
    CHECK_EQ(g.lang, 1);
    CHECK_EQ(g.mode, MODE_MARATHON);
    CHECK_EQ(g.status, STATUS_PLAYING);
    CHECK_EQ(g.n_guesses, 0);
    CHECK_EQ(g.cur_len, 0);
    CHECK_MEM(g.target, "CRANE", WORD_LEN);
    for (int i = 0; i < 26; i++) CHECK_EQ(g.key_state[i], FB_NONE);
}

TEST(type_letters_up_to_five)
{
    GameState g;
    game_init(&g, 0, MODE_CLASSIC, "CRANE");
    CHECK(game_type_letter(&g, 'S'));
    CHECK(game_type_letter(&g, 'P'));
    CHECK(game_type_letter(&g, 'E'));
    CHECK(game_type_letter(&g, 'E'));
    CHECK(game_type_letter(&g, 'D'));
    CHECK(!game_type_letter(&g, 'X'));       // row full
    CHECK_EQ(g.cur_len, 5);
    CHECK_MEM(g.current, "SPEED", WORD_LEN);
}

TEST(type_rejects_non_letters)
{
    GameState g;
    game_init(&g, 0, MODE_CLASSIC, "CRANE");
    CHECK(!game_type_letter(&g, 'a'));
    CHECK(!game_type_letter(&g, '1'));
    CHECK(!game_type_letter(&g, KEY_ENTER));
    CHECK(!game_type_letter(&g, KEY_DEL));
    CHECK_EQ(g.cur_len, 0);
}

TEST(backspace)
{
    GameState g;
    game_init(&g, 0, MODE_CLASSIC, "CRANE");
    CHECK(!game_backspace(&g));              // nothing to delete
    type(&g, "ABC");
    CHECK(game_backspace(&g));
    CHECK_EQ(g.cur_len, 2);
    CHECK_EQ(g.current[2], 0);
    CHECK(game_backspace(&g));
    CHECK(game_backspace(&g));
    CHECK(!game_backspace(&g));
    CHECK_EQ(g.cur_len, 0);
}

TEST(submit_too_short)
{
    GameState g;
    game_init(&g, 0, MODE_CLASSIC, "CRANE");
    CHECK_EQ(game_submit(&g, &lang), SUBMIT_TOO_SHORT);
    type(&g, "SPEE");
    CHECK_EQ(game_submit(&g, &lang), SUBMIT_TOO_SHORT);
    CHECK_EQ(g.n_guesses, 0);
    CHECK_EQ(g.cur_len, 4);                  // row kept for editing
}

TEST(submit_not_in_list_keeps_row)
{
    GameState g;
    game_init(&g, 0, MODE_CLASSIC, "CRANE");
    type(&g, "ZZZZZ");
    CHECK_EQ(game_submit(&g, &lang), SUBMIT_NOT_IN_LIST);
    CHECK_EQ(g.n_guesses, 0);
    CHECK_EQ(g.cur_len, 5);
    CHECK_EQ(g.key_state['Z' - 'A'], FB_NONE);
}

TEST(submit_accepted_row)
{
    GameState g;
    game_init(&g, 0, MODE_CLASSIC, "CRANE");
    type(&g, "SPEED");
    CHECK_EQ(game_submit(&g, &lang), SUBMIT_OK);
    CHECK_EQ(g.n_guesses, 1);
    CHECK_EQ(g.cur_len, 0);
    CHECK_MEM(g.guesses[0], "SPEED", WORD_LEN);
    CHECK_EQ(g.feedback[0][2], FB_PRESENT);  // E
    CHECK_EQ(g.feedback[0][3], FB_ABSENT);   // second E: nothing left to claim
    CHECK_EQ(g.key_state['E' - 'A'], FB_PRESENT);
    CHECK_EQ(g.key_state['S' - 'A'], FB_ABSENT);
    CHECK_EQ(g.key_state['C' - 'A'], FB_NONE);
}

TEST(win_on_second_guess)
{
    GameState g;
    game_init(&g, 0, MODE_CLASSIC, "CRANE");
    type(&g, "SPEED");
    game_submit(&g, &lang);
    type(&g, "CRANE");
    CHECK_EQ(game_submit(&g, &lang), SUBMIT_WON);
    CHECK_EQ(g.status, STATUS_WON);
    CHECK_EQ(g.n_guesses, 2);
    for (int i = 0; i < WORD_LEN; i++) CHECK_EQ(g.feedback[1][i], FB_CORRECT);
    // game over: no more input
    CHECK(!game_type_letter(&g, 'A'));
    CHECK(!game_backspace(&g));
    CHECK_EQ(game_submit(&g, &lang), SUBMIT_TOO_SHORT);
}

TEST(win_on_first_guess)
{
    GameState g;
    game_init(&g, 0, MODE_CLASSIC, "CRANE");
    type(&g, "CRANE");
    CHECK_EQ(game_submit(&g, &lang), SUBMIT_WON);
    CHECK_EQ(g.n_guesses, 1);
}

TEST(lose_after_six_guesses)
{
    GameState g;
    game_init(&g, 0, MODE_CLASSIC, "CRANE");
    for (int r = 0; r < MAX_GUESSES; r++) {
        type(&g, "APPLE");
        SubmitResult res = game_submit(&g, &lang);
        CHECK_EQ(res, r == MAX_GUESSES - 1 ? SUBMIT_LOST : SUBMIT_OK);
    }
    CHECK_EQ(g.status, STATUS_LOST);
    CHECK_EQ(g.n_guesses, MAX_GUESSES);
    CHECK(!game_type_letter(&g, 'A'));
}

TEST(win_on_sixth_guess)
{
    GameState g;
    game_init(&g, 0, MODE_CLASSIC, "CRANE");
    for (int r = 0; r < MAX_GUESSES - 1; r++) { type(&g, "APPLE"); game_submit(&g, &lang); }
    type(&g, "CRANE");
    CHECK_EQ(game_submit(&g, &lang), SUBMIT_WON);
    CHECK_EQ(g.status, STATUS_WON);
}

TEST(key_state_never_downgrades)
{
    GameState g;
    game_init(&g, 0, MODE_CLASSIC, "STEEL");
    game_replay_guess(&g, "LEVEL");          // L exact at the end
    CHECK_EQ(g.key_state['L' - 'A'], FB_CORRECT);
    CHECK_EQ(g.key_state['E' - 'A'], FB_CORRECT);
    game_replay_guess(&g, "ALLEY");          // L only present here
    CHECK_EQ(g.key_state['L' - 'A'], FB_CORRECT);
    CHECK_EQ(g.key_state['A' - 'A'], FB_ABSENT);
    CHECK_EQ(g.n_guesses, 2);
}

TEST(key_state_upgrades_absent_to_present_to_correct)
{
    GameState g;
    game_init(&g, 0, MODE_CLASSIC, "CRANE");
    game_replay_guess(&g, "SPEED");          // E present
    CHECK_EQ(g.key_state['E' - 'A'], FB_PRESENT);
    game_replay_guess(&g, "SPACE");          // E exact
    CHECK_EQ(g.key_state['E' - 'A'], FB_CORRECT);
}

TEST(replay_ignored_after_game_over)
{
    GameState g;
    game_init(&g, 0, MODE_CLASSIC, "CRANE");
    game_replay_guess(&g, "CRANE");
    CHECK_EQ(g.status, STATUS_WON);
    game_replay_guess(&g, "SPEED");
    CHECK_EQ(g.n_guesses, 1);
}

static const TestCase logic_tests[] = {
    T(score_exact_match), T(score_no_common_letter), T(score_single_present),
    T(score_present_and_correct), T(score_anagram), T(score_all_shifted),
    T(score_double_guess_single_target), T(score_double_guess_both_present),
    T(score_exact_wins_over_present), T(score_two_present_one_exact),
    T(score_five_same_letters), T(score_triple_letter_target),
    T(score_guess_more_copies_than_target), T(score_never_overclaims),
    T(valid_first_and_last_entries), T(valid_middle_entry), T(valid_rejects_unknown),
    T(valid_rejects_near_miss), T(valid_empty_list), T(valid_single_entry_list),
    T(init_resets_everything), T(type_letters_up_to_five), T(type_rejects_non_letters),
    T(backspace), T(submit_too_short), T(submit_not_in_list_keeps_row), T(submit_accepted_row),
    T(win_on_second_guess), T(win_on_first_guess), T(lose_after_six_guesses), T(win_on_sixth_guess),
    T(key_state_never_downgrades), T(key_state_upgrades_absent_to_present_to_correct),
    T(replay_ignored_after_game_over),
};
SUITE(logic)
