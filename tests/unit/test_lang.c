// Language table and the generated word lists (build/gen/wordlist_*.c).
#include "test.h"
#include "lang.h"
#include "layout.h"

static bool upper_word(const char *w)
{
    for (int i = 0; i < WORD_LEN; i++)
        if (w[i] < 'A' || w[i] > 'Z') return false;
    return true;
}

TEST(both_languages_have_lists)
{
    for (int l = 0; l < LANG_COUNT; l++) {
        CHECK(languages[l].n_solutions >= 400);
        CHECK(languages[l].n_valid >= 4000);
        CHECK(languages[l].n_valid > languages[l].n_solutions);
    }
}

TEST(words_are_upper_case_letters)
{
    for (int l = 0; l < LANG_COUNT; l++) {
        const Language *L = &languages[l];
        for (int i = 0; i < L->n_solutions; i++) CHECK(upper_word(L->solutions[i]));
        for (int i = 0; i < L->n_valid; i++) CHECK(upper_word(L->valid[i]));
    }
}

TEST(valid_list_is_strictly_sorted)      // binary search relies on it
{
    for (int l = 0; l < LANG_COUNT; l++) {
        const Language *L = &languages[l];
        for (int i = 1; i < L->n_valid; i++)
            CHECK(memcmp(L->valid[i - 1], L->valid[i], WORD_LEN) < 0);
    }
}

TEST(solutions_are_unique)
{
    for (int l = 0; l < LANG_COUNT; l++) {
        const Language *L = &languages[l];
        for (int i = 0; i < L->n_solutions; i++)
            for (int j = i + 1; j < L->n_solutions; j++)
                CHECK(memcmp(L->solutions[i], L->solutions[j], WORD_LEN) != 0);
    }
}

TEST(every_solution_is_a_valid_guess)
{
    for (int l = 0; l < LANG_COUNT; l++) {
        const Language *L = &languages[l];
        for (int i = 0; i < L->n_solutions; i++) {
            // binary search by hand (logic.c is tested separately)
            int lo = 0, hi = L->n_valid - 1, found = 0;
            while (lo <= hi) {
                int mid = (lo + hi) / 2;
                int c = memcmp(L->valid[mid], L->solutions[i], WORD_LEN);
                if (c == 0) { found = 1; break; }
                if (c < 0) lo = mid + 1; else hi = mid - 1;
            }
            CHECK(found);
        }
    }
}

TEST(keyboard_has_every_letter_once_plus_enter_and_delete)
{
    for (int l = 0; l < LANG_COUNT; l++) {
        int seen[256] = { 0 };
        int total = 0;
        for (int r = 0; r < KB_ROWS; r++)
            for (const char *p = languages[l].kb_rows[r]; *p; p++) { seen[(u8)*p]++; total++; }
        for (char c = 'A'; c <= 'Z'; c++) CHECK_EQ(seen[(u8)c], 1);
        CHECK_EQ(seen[KEY_ENTER], 1);
        CHECK_EQ(seen[KEY_DEL], 1);
        CHECK_EQ(total, 28);
        for (int r = 0; r < KB_ROWS; r++) CHECK(strlen(languages[l].kb_rows[r]) <= KB_MAX_KEYS);
    }
}

TEST(keyboard_layouts_differ_per_language)
{
    CHECK_EQ(languages[LANG_FR].kb_rows[0][0], 'A');   // AZERTY
    CHECK_EQ(languages[LANG_EN].kb_rows[0][0], 'Q');   // QWERTY
}

TEST(ui_strings_fit_the_screen)
{
    for (int l = 0; l < LANG_COUNT; l++) {
        const Language *L = &languages[l];
        const char *full_width[] = {
            L->name, L->press_start, L->menu_help, L->mode_classic, L->msg_too_short,
            L->msg_not_in_list, L->stats_title, L->stats_distribution, L->stats_back,
            L->result_prompt, L->marathon_over, L->new_record, L->quit_question,
            L->quit_choices, L->stats_classic, L->options_title, L->select_help, L->ta_done, L->enter_initials, L->initials_help,
            L->records_title, L->records_help,
        };
        for (unsigned i = 0; i < sizeof full_width / sizeof full_width[0]; i++) {
            CHECK(full_width[i] != NULL);
            CHECK(strlen(full_width[i]) <= SCREEN_TW);
        }
        for (int i = 0; i < MAX_GUESSES; i++) {
            CHECK(L->win_msgs[i] != NULL);
            CHECK(strlen(L->win_msgs[i]) <= SCREEN_TW);
        }
        CHECK(strlen(L->lose_msg) + WORD_LEN <= SCREEN_TW);
        // main menu lines are centred with "> " and " <" around them
        const char *menu[] = { L->menu_classic, L->menu_marathon, L->menu_time_attack,
                               L->menu_records, L->menu_stats, L->menu_options };
        for (unsigned i = 0; i < sizeof menu / sizeof menu[0]; i++)
            CHECK(strlen(menu[i]) + 4 <= SCREEN_TW);
        // option labels sit in columns 5..16, values from column 19 with a ">" after
        CHECK(strlen(L->opt_language) <= 11);
        CHECK(strlen(L->opt_sound) <= 11);
        CHECK(strlen(L->name) <= 9);
        CHECK(strlen(L->on) <= 9);
        CHECK(strlen(L->off) <= 9);
        // mode select: difficulty / "NN WORDS" in 5..16, record from 17
        for (int d = 0; d < DIFF_COUNT; d++) CHECK(strlen(L->difficulty[d]) <= 11);
        CHECK(3 + strlen(L->words) <= 11);
        CHECK(strlen(L->best) + 1 + 5 <= SCREEN_TW - 17);
        // records page title: "TIME ATTACK 15 WORDS"
        CHECK(strlen(L->menu_time_attack) + 4 + strlen(L->words) <= SCREEN_TW - 4);
        // time attack result lines
        CHECK(strlen(L->time) + 1 + 8 <= SCREEN_TW);
        CHECK(strlen(L->penalties) + 1 + 2 + 1 + 4 <= SCREEN_TW);
        // modal box: 21 tiles wide, text lines inside must leave the frame
        CHECK(strlen(L->quit_question) <= 17);
        CHECK(strlen(L->quit_choices) <= 17);
        // statistics: "WORDS 123" from column 2, "FOUND 100%" from column 16
        CHECK(3 + strlen(L->stats_played) + 5 <= 16);
        CHECK(17 + strlen(L->stats_win_rate) + 4 <= SCREEN_TW);
        CHECK(3 + strlen(L->stats_classic) + 5 <= SCREEN_TW);
    }
}

TEST(ui_strings_use_only_font_characters)
{
    static const char font[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?:.-/%><#',";
    for (int l = 0; l < LANG_COUNT; l++) {
        const Language *L = &languages[l];
        const char *all[] = {
            L->name, L->press_start, L->menu_classic, L->menu_marathon, L->menu_time_attack,
            L->menu_records, L->menu_stats, L->menu_options, L->menu_help, L->options_title,
            L->opt_language, L->opt_sound, L->on, L->off, L->difficulty[0], L->difficulty[1],
            L->words, L->best, L->select_help, L->mode_classic, L->score, L->word,
            L->msg_too_short, L->msg_not_in_list, L->lose_msg, L->quit_question, L->quit_choices,
            L->stats_classic,
            L->result_prompt, L->marathon_over, L->new_record, L->ta_done, L->time, L->penalties,
            L->enter_initials, L->initials_help, L->records_title, L->records_help,
            L->stats_title, L->stats_played, L->stats_win_rate, L->stats_streak, L->stats_max_streak,
            L->stats_distribution, L->stats_back,
            L->win_msgs[0], L->win_msgs[1], L->win_msgs[2], L->win_msgs[3], L->win_msgs[4], L->win_msgs[5],
        };
        for (unsigned i = 0; i < sizeof all / sizeof all[0]; i++)
            for (const char *p = all[i]; *p; p++)
                CHECK(strchr(font, *p) != NULL);
    }
}

static const TestCase lang_tests[] = {
    T(both_languages_have_lists), T(words_are_upper_case_letters), T(valid_list_is_strictly_sorted),
    T(solutions_are_unique), T(every_solution_is_a_valid_guess),
    T(keyboard_has_every_letter_once_plus_enter_and_delete), T(keyboard_layouts_differ_per_language),
    T(ui_strings_fit_the_screen), T(ui_strings_use_only_font_characters),
};
SUITE(lang)
