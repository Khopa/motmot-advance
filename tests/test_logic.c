// Host-side unit tests for the platform-independent game logic.
// Build & run:  make test
#include <stdio.h>
#include <string.h>
#include "logic.h"

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
} while (0)

// tiny sorted list acting as a language
static const char words[][WORD_LEN] __attribute__((nonstring)) = {
    "ABBEY", "ALLEY", "APPLE", "CRANE", "EERIE", "LEVEL", "SPEED", "STEEL", "TESTS",
};
static const Language lang = {
    .name = "TEST", .solutions = words, .n_solutions = 3,
    .valid = words, .n_valid = sizeof words / sizeof words[0],
};

static void check_score(const char *guess, const char *target, const char *expect)
{
    // expect: 'G' correct, 'Y' present, '.' absent
    u8 out[WORD_LEN];
    logic_score(guess, target, out);
    char got[WORD_LEN + 1];
    for (int i = 0; i < WORD_LEN; i++)
        got[i] = out[i] == FB_CORRECT ? 'G' : out[i] == FB_PRESENT ? 'Y' : '.';
    got[WORD_LEN] = 0;
    if (strcmp(got, expect) != 0) {
        printf("FAIL score(%s, %s) = %s, expected %s\n", guess, target, got, expect);
        failures++;
    }
}

int main(void)
{
    // --- scoring, including repeated letters ---
    check_score("CRANE", "CRANE", "GGGGG");
    check_score("CRANE", "SPEED", "....Y");
    check_score("SPEED", "ABBEY", "...G.");   // one E in target, matched exactly
    check_score("EERIE", "SPEED", "YY...");   // both target E claimed by E@0,E@1; E@4 absent
    check_score("LEVEL", "STEEL", ".Y.GG");   // L@0 absent: the only L is matched exactly at 4
    check_score("ALLEY", "LEVEL", ".YYG.");   // two L present, E@3 exact
    check_score("TESTS", "STEEL", "YYY..");   // second T and S have nothing left to claim
    check_score("AAAAA", "ABBEY", "G....");

    // --- validation (binary search) ---
    CHECK(logic_is_valid_guess(&lang, "APPLE"));
    CHECK(logic_is_valid_guess(&lang, "ABBEY"));
    CHECK(logic_is_valid_guess(&lang, "TESTS"));
    CHECK(!logic_is_valid_guess(&lang, "ZZZZZ"));
    CHECK(!logic_is_valid_guess(&lang, "AAAAA"));

    // --- a full round ---
    GameState g;
    game_init(&g, 0, MODE_CLASSIC, "CRANE");
    CHECK(game_submit(&g, &lang) == SUBMIT_TOO_SHORT);
    for (const char *p = "SPEE"; *p; p++) CHECK(game_type_letter(&g, *p));
    CHECK(g.cur_len == 4);
    CHECK(game_submit(&g, &lang) == SUBMIT_TOO_SHORT);
    CHECK(game_type_letter(&g, 'D'));
    CHECK(!game_type_letter(&g, 'X'));        // row full
    CHECK(game_submit(&g, &lang) == SUBMIT_OK);
    CHECK(g.n_guesses == 1 && g.cur_len == 0);
    CHECK(g.feedback[0][2] == FB_PRESENT && g.feedback[0][3] == FB_ABSENT);
    CHECK(g.key_state['E' - 'A'] == FB_PRESENT);
    CHECK(g.key_state['S' - 'A'] == FB_ABSENT);
    CHECK(g.key_state['C' - 'A'] == FB_NONE);

    for (const char *p = "ZZZZZ"; *p; p++) game_type_letter(&g, *p);
    CHECK(game_submit(&g, &lang) == SUBMIT_NOT_IN_LIST);
    CHECK(g.n_guesses == 1);
    for (int i = 0; i < 5; i++) game_backspace(&g);
    CHECK(!game_backspace(&g));

    for (const char *p = "CRANE"; *p; p++) game_type_letter(&g, *p);
    CHECK(game_submit(&g, &lang) == SUBMIT_WON);
    CHECK(g.status == STATUS_WON && g.n_guesses == 2);
    CHECK(!game_type_letter(&g, 'A'));        // game over

    // --- losing ---
    game_init(&g, 0, MODE_CLASSIC, "CRANE");
    for (int r = 0; r < MAX_GUESSES; r++) {
        for (const char *p = "APPLE"; *p; p++) game_type_letter(&g, *p);
        SubmitResult res = game_submit(&g, &lang);
        CHECK(res == (r == MAX_GUESSES - 1 ? SUBMIT_LOST : SUBMIT_OK));
    }
    CHECK(g.status == STATUS_LOST);

    // --- keyboard colours never downgrade ---
    game_init(&g, 0, MODE_CLASSIC, "STEEL");
    game_replay_guess(&g, "LEVEL");           // L@4 correct
    CHECK(g.key_state['L' - 'A'] == FB_CORRECT);
    game_replay_guess(&g, "ALLEY");           // L present only
    CHECK(g.key_state['L' - 'A'] == FB_CORRECT);
    CHECK(g.n_guesses == 2);

    if (failures == 0) printf("All logic tests passed.\n");
    else printf("%d failure(s)\n", failures);
    return failures != 0;
}
