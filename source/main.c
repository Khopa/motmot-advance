// MotMot Advance — entry point and screen state machine.
//
//   language (once, at boot) -> title -> main menu
//   main menu -> classic game -> result
//             -> mode select (marathon difficulty / time attack length) -> game
//             -> records, statistics, options
#include <string.h>
#include "common.h"
#include "game_state.h"
#include "input.h"
#include "keyboard.h"
#include "lang.h"
#include "logic.h"
#include "render.h"
#include "rng.h"
#include "sound.h"
#include "stats.h"
#include "time_attack.h"

typedef enum {
    SCR_TITLE, SCR_LANG, SCR_MENU, SCR_OPTIONS, SCR_RECORDS, SCR_MODE_SELECT,
    SCR_GAME, SCR_RESULT, SCR_MARATHON_RESULT, SCR_TA_RESULT, SCR_STATS
} Screen;

enum { MENU_CLASSIC, MENU_MARATHON, MENU_TIME_ATTACK, MENU_RECORDS, MENU_STATS, MENU_OPTIONS, MENU_COUNT };
enum { OPT_LANGUAGE, OPT_SOUND, OPT_COUNT };
enum { RECORDS_PAGES = TA_LENGTH_COUNT + 1 };     // one per time attack length + marathon

static GameState        game;
static KbCursor         kb;
static MarathonState    marathon;
static TimeAttackState  ta;
volatile u8             current_screen;     // Screen being run; read by tests/emu through RAM
static u32              frames;             // since power-on; entropy for the RNG
static u8               menu_lang;          // language shown/selected in the menus
static int              menu_item;          // main menu cursor
static int              sub_item;           // cursor of the options / mode select screens
static int              records_page;
static u8               next_mode;          // mode the mode-select screen prepares

static const u8 marathon_hp[DIFF_COUNT] = { 3, 5 };

#define LANG()  (&languages[game.lang])
#define MLANG() (&languages[menu_lang])

// ---------------------------------------------------------------------------
// Frame loop
// ---------------------------------------------------------------------------

static void next_frame(void)
{
    VBlankIntrWait();
    render_vblank();
    sound_update();
    input_poll();
    frames++;
    if (ta.running && ta.frames < TA_MAX_FRAMES) ta.frames++;
}

static void wait_frames(int n)
{
    while (n-- > 0) next_frame();
}

// wait up to n frames, less if a button is pressed
static void wait_or_key(int n)
{
    while (n-- > 0) {
        next_frame();
        if (input_hit(KEY_A | KEY_B | KEY_START)) return;
    }
}

// ---------------------------------------------------------------------------
// Shared drawing
// ---------------------------------------------------------------------------

static void draw_logo(int ty)
{
    static const char logo[] = "MOTMOT";
    static const u8 pals[] = { PAL_CORRECT, PAL_PRESENT, PAL_ABSENT,
                               PAL_CORRECT, PAL_PRESENT, PAL_ABSENT };
    for (int i = 0; i < 6; i++)
        cell_draw(9 + i * 2, ty, logo[i], pals[i]);
    txt_center(ty + 2, "ADVANCE", PAL_TXT_WHITE);
}

// A centred menu line; the selected one is yellow between "> " and " <".
static void draw_menu_line(int ty, const char *text, bool selected)
{
    txt_center_marked(ty, text, selected ? PAL_TXT_YELLOW : PAL_TXT_WHITE, selected);
}

// Label at the left, value between < > at the right (options / mode select)
static void draw_option_line(int ty, const char *label, const char *value, bool selected)
{
    txt_clear_row(ty);
    txt_puts(5, ty, label, selected ? PAL_TXT_YELLOW : PAL_TXT_WHITE);
    if (selected) txt_puts(3, ty, ">", PAL_TXT_GREEN);
    txt_puts(17, ty, "<", PAL_TXT_GRAY);
    txt_puts(19, ty, value, PAL_TXT_WHITE);
    txt_puts(20 + strlen(value), ty, ">", PAL_TXT_GRAY);
}

static void draw_time(int tx, int ty, const TimeRecord *r, int pal)
{
    char buf[9];
    if (r && r->used) ta_format_time(r->frames, buf);
    else memcpy(buf, "--:--.--", 9);
    txt_puts(tx, ty, buf, pal);
}

// ---------------------------------------------------------------------------
// Title and language
// ---------------------------------------------------------------------------

static Screen title_screen(void)
{
    render_clear();
    decor_show(true);
    draw_logo(5);
    txt_center(18, "KHOPA - 2026", PAL_TXT_GRAY);

    for (;;) {
        next_frame();
        if ((frames & 31) == 0) txt_center(13, MLANG()->press_start, PAL_TXT_WHITE);
        if ((frames & 31) == 20) txt_clear_row(13);
        if (input_hit(KEY_START | KEY_A)) { sfx_play(SFX_SELECT); return SCR_MENU; }
    }
}

static void draw_lang_choice(void)
{
    for (int i = 0; i < LANG_COUNT; i++)
        draw_menu_line(7 + i * 2, languages[i].name, i == menu_lang);
}

// Shown once at boot; the language stays changeable in the options
static Screen lang_screen(void)
{
    render_clear();
    draw_logo(1);
    txt_center(5, "LANGUE / LANGUAGE", PAL_TXT_GRAY);
    txt_center(18, "A: OK", PAL_TXT_DIM);
    draw_lang_choice();

    for (;;) {
        next_frame();
        int move = 0;
        if (input_nav(KEY_UP) || input_nav(KEY_LEFT))    move = -1;
        if (input_nav(KEY_DOWN) || input_nav(KEY_RIGHT)) move = 1;
        if (move) {
            menu_lang = (menu_lang + move + LANG_COUNT) % LANG_COUNT;
            sfx_play(SFX_MOVE);
            draw_lang_choice();
        }
        if (input_hit(KEY_A | KEY_START)) {
            save.lang = menu_lang;
            stats_save();
            sfx_play(SFX_SELECT);
            menu_item = MENU_CLASSIC;
            return SCR_TITLE;
        }
    }
}

// ---------------------------------------------------------------------------
// Main menu
// ---------------------------------------------------------------------------

static const char *menu_label(int item)
{
    const Language *L = MLANG();
    switch (item) {
    case MENU_CLASSIC:     return L->menu_classic;
    case MENU_MARATHON:    return L->menu_marathon;
    case MENU_TIME_ATTACK: return L->menu_time_attack;
    case MENU_RECORDS:     return L->menu_records;
    case MENU_STATS:       return L->menu_stats;
    default:               return L->menu_options;
    }
}

static void draw_menu(void)
{
    for (int i = 0; i < MENU_COUNT; i++)
        draw_menu_line(6 + i * 2, menu_label(i), i == menu_item);
}

static Screen menu_screen(void)
{
    render_clear();
    draw_logo(1);
    txt_center(18, MLANG()->menu_help, PAL_TXT_DIM);
    draw_menu();

    for (;;) {
        next_frame();
        int move = 0;
        if (input_nav(KEY_UP))   move = -1;
        if (input_nav(KEY_DOWN)) move = 1;
        if (move) {
            menu_item = (menu_item + move + MENU_COUNT) % MENU_COUNT;
            sfx_play(SFX_MOVE);
            draw_menu();
        }
        if (input_hit(KEY_A | KEY_START)) {
            sfx_play(SFX_SELECT);
            sub_item = 0;
            switch (menu_item) {
            case MENU_CLASSIC:     game.mode = MODE_CLASSIC;  return SCR_GAME;
            case MENU_MARATHON:    next_mode = MODE_MARATHON; sub_item = save.marathon_diff; return SCR_MODE_SELECT;
            case MENU_TIME_ATTACK: next_mode = MODE_TIME_ATTACK; sub_item = save.ta_length; return SCR_MODE_SELECT;
            case MENU_RECORDS:     return SCR_RECORDS;
            case MENU_STATS:       return SCR_STATS;
            default:               return SCR_OPTIONS;
            }
        }
        if (input_hit(KEY_B)) return SCR_TITLE;
    }
}

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

static void draw_options(void)
{
    const Language *L = MLANG();
    txt_clear();
    draw_logo(1);
    txt_center(6, L->options_title, PAL_TXT_YELLOW);
    draw_option_line(9, L->opt_language, L->name, sub_item == OPT_LANGUAGE);
    draw_option_line(11, L->opt_sound, save.sound_on ? L->on : L->off, sub_item == OPT_SOUND);
    txt_center(18, L->menu_help, PAL_TXT_DIM);
}

static Screen options_screen(void)
{
    render_clear();
    draw_options();

    for (;;) {
        next_frame();
        bool dirty = false;
        if (input_nav(KEY_UP) || input_nav(KEY_DOWN)) {
            sub_item = (sub_item + 1) % OPT_COUNT;      // two options: any direction toggles
            sfx_play(SFX_MOVE);
            dirty = true;
        }
        if (input_hit(KEY_LEFT | KEY_RIGHT | KEY_A)) {
            if (sub_item == OPT_LANGUAGE) {             // LEFT: previous, RIGHT / A: next
                int step = input_hit(KEY_LEFT) ? LANG_COUNT - 1 : 1;
                menu_lang = (menu_lang + step) % LANG_COUNT;
                save.lang = menu_lang;
                sfx_play(SFX_MOVE);
            } else {
                save.sound_on = !save.sound_on;
                sound_set_enabled(save.sound_on);
                sfx_play(SFX_SELECT);
            }
            stats_save();
            dirty = true;
        }
        if (dirty) draw_options();
        if (input_hit(KEY_B | KEY_START)) { sfx_play(SFX_DELETE); return SCR_MENU; }
    }
}

// ---------------------------------------------------------------------------
// Mode selection: Marathon difficulty or Time Attack length, with the records
// ---------------------------------------------------------------------------

static void draw_mode_select(void)
{
    const Language *L = MLANG();
    txt_clear();
    draw_logo(1);
    if (next_mode == MODE_MARATHON) {
        txt_center(6, L->menu_marathon, PAL_TXT_YELLOW);
        for (int d = 0; d < DIFF_COUNT; d++) {
            int ty = 9 + d * 2;
            bool sel = sub_item == d;
            txt_puts(5, ty, L->difficulty[d], sel ? PAL_TXT_YELLOW : PAL_TXT_WHITE);
            if (sel) txt_puts(3, ty, ">", PAL_TXT_GREEN);
            txt_puts(17, ty, L->best, PAL_TXT_GRAY);
            txt_uint(18 + strlen(L->best), ty, save.marathon_best[d], PAL_TXT_WHITE);
        }
    } else {
        txt_center(6, L->menu_time_attack, PAL_TXT_YELLOW);
        for (int i = 0; i < TA_LENGTH_COUNT; i++) {
            int ty = 9 + i * 2;
            bool sel = sub_item == i;
            int pal = sel ? PAL_TXT_YELLOW : PAL_TXT_WHITE;
            int x = 5 + txt_uint(5, ty, ta_word_counts[i], pal);
            txt_puts(x + 1, ty, L->words, pal);
            if (sel) txt_puts(3, ty, ">", PAL_TXT_GREEN);
            draw_time(19, ty, &save.ta_board[i][0], PAL_TXT_GRAY);
        }
    }
    txt_center(18, L->select_help, PAL_TXT_DIM);
}

static Screen mode_select_screen(void)
{
    int count = next_mode == MODE_MARATHON ? DIFF_COUNT : TA_LENGTH_COUNT;
    render_clear();
    draw_mode_select();

    for (;;) {
        next_frame();
        int move = 0;
        if (input_nav(KEY_UP))   move = -1;
        if (input_nav(KEY_DOWN)) move = 1;
        if (move) {
            sub_item = (sub_item + move + count) % count;
            sfx_play(SFX_MOVE);
            draw_mode_select();
        }
        if (input_hit(KEY_A | KEY_START)) {
            sfx_play(SFX_SELECT);
            if (next_mode == MODE_MARATHON) save.marathon_diff = sub_item;
            else                            save.ta_length = sub_item;
            stats_save();
            game.mode = next_mode;
            return SCR_GAME;
        }
        if (input_hit(KEY_B)) { sfx_play(SFX_DELETE); return SCR_MENU; }
    }
}

// ---------------------------------------------------------------------------
// Records: one page per Time Attack length, one for the Marathon
// ---------------------------------------------------------------------------

static void draw_leaderboard(int ty, const TimeRecord board[TA_TOP], int highlight)
{
    for (int i = 0; i < TA_TOP; i++, ty += 2) {
        int pal = i == highlight ? PAL_TXT_GREEN : PAL_TXT_WHITE;
        txt_uint(8, ty, i + 1, PAL_TXT_GRAY);
        txt_puts(9, ty, ".", PAL_TXT_GRAY);
        if (board[i].used) {
            char ini[4] = { board[i].initials[0], board[i].initials[1], board[i].initials[2], 0 };
            txt_puts(11, ty, ini, pal);
            draw_time(15, ty, &board[i], pal);
        } else {
            txt_puts(11, ty, "---", PAL_TXT_DIM);
            draw_time(15, ty, NULL, PAL_TXT_DIM);
        }
    }
}

static void draw_records(void)
{
    const Language *L = MLANG();
    txt_clear();
    txt_center(1, L->records_title, PAL_TXT_YELLOW);
    txt_puts(2, 3, "<", PAL_TXT_GRAY);
    txt_puts(27, 3, ">", PAL_TXT_GRAY);
    if (records_page < TA_LENGTH_COUNT) {
        char title[24];
        int n = ta_word_counts[records_page];
        int p = 0;
        for (const char *s = L->menu_time_attack; *s; s++) title[p++] = *s;
        title[p++] = ' ';
        if (n >= 10) title[p++] = '0' + n / 10;
        title[p++] = '0' + n % 10;
        title[p++] = ' ';
        for (const char *s = L->words; *s; s++) title[p++] = *s;
        title[p] = 0;
        txt_center(3, title, PAL_TXT_WHITE);
        draw_leaderboard(6, save.ta_board[records_page], -1);
    } else {
        txt_center(3, L->menu_marathon, PAL_TXT_WHITE);
        for (int d = 0; d < DIFF_COUNT; d++) {
            int ty = 6 + d * 2;
            txt_puts(8, ty, L->difficulty[d], PAL_TXT_GRAY);
            txt_uint(19, ty, save.marathon_best[d], PAL_TXT_WHITE);
        }
    }
    txt_center(18, L->records_help, PAL_TXT_DIM);
}

static Screen records_screen(void)
{
    render_clear();
    draw_records();
    for (;;) {
        next_frame();
        int move = 0;
        if (input_nav(KEY_LEFT))  move = -1;
        if (input_nav(KEY_RIGHT)) move = 1;
        if (move) {
            records_page = (records_page + move + RECORDS_PAGES) % RECORDS_PAGES;
            sfx_play(SFX_MOVE);
            draw_records();
        }
        if (input_hit(KEY_B | KEY_A | KEY_START)) { sfx_play(SFX_DELETE); return SCR_MENU; }
    }
}

// ---------------------------------------------------------------------------
// Game
// ---------------------------------------------------------------------------

// Pick a random solution, avoiding the last few played in this language.
static void random_word(u8 lang_id, u8 mode)
{
    const Language *L = &languages[lang_id];
    rng_seed(save.rng_state ^ (frames * 2654435761u));
    u16 idx;
    int tries = 0;
    do {
        idx = rng_range(L->n_solutions);
    } while (stats_recently_played(lang_id, idx) && ++tries < 64);
    stats_push_recent(lang_id, idx);
    save.rng_state = rng_state();
    stats_save();
    game_init(&game, lang_id, mode, L->solutions[idx]);
}

static void start_game(u8 lang_id, u8 mode)
{
    if (mode == MODE_MARATHON) {
        marathon.difficulty = save.marathon_diff;
        marathon.hp_max = marathon.hp = marathon_hp[marathon.difficulty];
        marathon.score = 0;
        marathon.new_record = false;
    } else if (mode == MODE_TIME_ATTACK) {
        memset(&ta, 0, sizeof ta);
        ta.length_idx = save.ta_length;
        ta.total = ta_word_counts[ta.length_idx];
        ta.rank = -1;
    }
    random_word(lang_id, mode);
    if (mode == MODE_TIME_ATTACK) ta.running = true;
}

static void draw_timer(void)
{
    char buf[9];
    ta_format_time(ta.frames, buf);
    txt_puts(SCREEN_TW - 9, MSG_TY, buf, PAL_TXT_WHITE);
}

// Row 0: mode name / score and hearts / word counter and timer
static void draw_mode_label(void)
{
    const Language *L = LANG();
    txt_clear_row(MSG_TY);
    if (game.mode == MODE_CLASSIC) {
        txt_center(MSG_TY, L->mode_classic, PAL_TXT_DIM);
    } else if (game.mode == MODE_MARATHON) {
        txt_puts(1, MSG_TY, L->score, PAL_TXT_GRAY);
        txt_uint(2 + strlen(L->score), MSG_TY, marathon.score, PAL_TXT_WHITE);
        int x = SCREEN_TW - 1 - marathon.hp_max;
        txt_fill(x, MSG_TY, marathon.hp, '\x04', PAL_TXT_YELLOW);
        txt_fill(x + marathon.hp, MSG_TY, marathon.hp_max - marathon.hp, '\x05', PAL_TXT_DIM);
    } else {
        int x = 1;
        txt_puts(x, MSG_TY, L->word, PAL_TXT_GRAY);
        x += strlen(L->word) + 1;
        x += txt_uint(x, MSG_TY, ta.done + 1 <= ta.total ? ta.done + 1 : ta.total, PAL_TXT_WHITE);
        txt_puts(x, MSG_TY, "/", PAL_TXT_GRAY);
        txt_uint(x + 1, MSG_TY, ta.total, PAL_TXT_WHITE);
        draw_timer();
    }
}

static void show_message(const char *msg, int pal)
{
    txt_clear_row(MSG_TY);
    txt_center(MSG_TY, msg, pal);
}

static void show_lose_message(void)
{
    const Language *L = LANG();
    char buf[32];
    int n = strlen(L->lose_msg);
    memcpy(buf, L->lose_msg, n);
    memcpy(buf + n, game.target, WORD_LEN);
    buf[n + WORD_LEN] = 0;
    show_message(buf, PAL_TXT_YELLOW);
}

static void update_cursor(void)
{
    int tx, ty;
    kb_key_pos(LANG(), kb.row, kb.col, &tx, &ty);
    cursor_set(tx * 8, ty * 8, true);
}

// Colour the freshly committed row cell by cell.
static void reveal_row(int row)
{
    static const SfxId reveal_sfx[] = { SFX_ABSENT, SFX_ABSENT, SFX_PRESENT, SFX_CORRECT };
    for (int c = 1; c <= WORD_LEN; c++) {
        render_grid_row(&game, row, c);
        sfx_play(reveal_sfx[game.feedback[row][c - 1]]);
        wait_frames(6);
    }
}

// SELECT: ask before leaving the game, in a modal box that hides the grid.
// Returns true to quit. (Never used in Time Attack: no pause there.)
static bool confirm_quit(void)
{
    const Language *L = LANG();
    modal_show(L->quit_question, L->quit_choices);
    cursor_set(0, 0, false);
    sfx_play(SFX_MOVE);
    for (;;) {
        next_frame();
        if (input_hit(KEY_A | KEY_START)) { sfx_play(SFX_SELECT); return true; }
        if (input_hit(KEY_B | KEY_SELECT)) {
            sfx_play(SFX_DELETE);
            modal_hide();
            update_cursor();
            return false;
        }
    }
}

// Start the next word of a chained mode on the same screen
static void next_word(void)
{
    random_word(game.lang, game.mode);
    render_grid(&game);
    render_keyboard(&game, LANG());
    draw_mode_label();
}

// Classic: record the result, show the verdict, wait a bit.
static void finish_game(void)
{
    const Language *L = LANG();
    bool won = game.status == STATUS_WON;

    stats_record_result(won, game.n_guesses);
    if (won) save.classic_won++;
    stats_save();

    if (won) {
        sfx_play(SFX_WIN);
        show_message(L->win_msgs[game.n_guesses - 1], PAL_TXT_GREEN);
    } else {
        sfx_play(SFX_LOSE);
        show_lose_message();
    }
    cursor_set(0, 0, false);
    wait_or_key(150);
}

static void marathon_lose_hp(void)
{
    if (marathon.hp > 0) marathon.hp--;
    sfx_play(SFX_HURT);
    draw_mode_label();
}

static void marathon_finish(void)
{
    u16 *best = &save.marathon_best[marathon.difficulty];
    marathon.new_record = marathon.score > *best;
    if (marathon.new_record) *best = marathon.score;
    stats_save();
}

// After an accepted guess in Marathon. Returns true when the run is over.
static bool marathon_after_guess(void)
{
    const Language *L = LANG();
    bool hard = marathon.difficulty == DIFF_HARD;

    // hard: every guess from the third one costs a life
    if (hard && game.n_guesses >= 3) {
        marathon_lose_hp();
        wait_frames(20);
    }

    if (game.status == STATUS_WON) {
        marathon.score++;
        draw_mode_label();
        sfx_play(SFX_WIN);
        show_message(L->win_msgs[game.n_guesses - 1], PAL_TXT_GREEN);
    } else if (game.status == STATUS_LOST) {
        if (!hard) marathon_lose_hp();          // easy: a missed word costs a life
        sfx_play(SFX_LOSE);
        show_lose_message();
    } else if (hard && marathon.hp == 0) {
        show_lose_message();                    // out of lives mid-word
        sfx_play(SFX_LOSE);
    } else {
        return false;                           // word still in progress
    }
    stats_record_result(game.status == STATUS_WON, game.n_guesses);   // global statistics

    wait_or_key(120);
    if (marathon.hp == 0) {
        marathon_finish();
        return true;
    }
    next_word();
    return false;
}

// After an accepted guess in Time Attack. Returns true when the run is over.
static bool time_attack_after_guess(void)
{
    const Language *L = LANG();
    if (game.status == STATUS_PLAYING) return false;

    ta.done++;
    stats_record_result(game.status == STATUS_WON, game.n_guesses);   // global statistics
    if (game.status == STATUS_WON) {
        sfx_play(ta.done == ta.total ? SFX_WIN : SFX_CORRECT);
        show_message(L->win_msgs[game.n_guesses - 1], PAL_TXT_GREEN);
        if (ta.done < ta.total) wait_frames(30);          // the clock keeps running
    } else {
        ta.missed++;
        ta.frames += TA_PENALTY_FRAMES;
        ta.running = false;                                // stopped while the word is shown
        sfx_play(SFX_LOSE);
        show_lose_message();
        wait_or_key(120);
        ta.running = ta.done < ta.total;
    }

    if (ta.done >= ta.total) {
        ta.running = false;
        ta.rank = ta_board_rank(save.ta_board[ta.length_idx], ta.frames);
        return true;
    }
    next_word();
    return false;
}

static Screen game_screen(void)
{
    start_game(menu_lang, game.mode);
    const Language *L = LANG();

    render_clear();
    render_grid(&game);
    render_keyboard(&game, L);
    txt_center(HELP_TY, L->game_help, PAL_TXT_DIM);
    kb.row = 0;
    kb.col = 0;
    update_cursor();
    draw_mode_label();
    int msg_timer = 0;

    for (;;) {
        next_frame();

        if (msg_timer > 0 && --msg_timer == 0) draw_mode_label();
        if (game.mode == MODE_TIME_ATTACK && msg_timer == 0) draw_timer();

        if (input_hit(KEY_SELECT)) {
            if (game.mode == MODE_TIME_ATTACK) {        // no pause against the clock:
                ta.running = false;                     // SELECT abandons the run at once
                sfx_play(SFX_LOSE);
                return SCR_MENU;
            }
            msg_timer = 0;
            if (!confirm_quit()) continue;
            if (game.mode == MODE_MARATHON) {           // the run ends here, score kept
                marathon_finish();
                return SCR_MARATHON_RESULT;
            }
            return SCR_MENU;                            // classic: abandoned, not counted
        }

        int dx = 0, dy = 0;
        if (input_nav(KEY_LEFT))  dx = -1;
        if (input_nav(KEY_RIGHT)) dx = 1;
        if (input_nav(KEY_UP))    dy = -1;
        if (input_nav(KEY_DOWN))  dy = 1;
        if (dx || dy) {
            kb_move(L, &kb, dx, dy);
            update_cursor();
        }

        char key = 0;
        if (input_hit(KEY_A))     key = kb_key_at(L, &kb);
        if (input_hit(KEY_B))     key = KEY_DEL;
        if (input_hit(KEY_START)) key = KEY_ENTER;
        if (!key) continue;

        if (key == KEY_DEL) {
            if (game_backspace(&game)) {
                render_grid_row(&game, game.n_guesses, 0);
                sfx_play(SFX_DELETE);
            }
            continue;
        }
        if (key != KEY_ENTER) {
            if (game_type_letter(&game, key)) {
                render_grid_row(&game, game.n_guesses, 0);
                sfx_play(SFX_KEY);
            }
            continue;
        }

        SubmitResult res = game_submit(&game, L);
        switch (res) {
        case SUBMIT_TOO_SHORT:
            show_message(L->msg_too_short, PAL_TXT_WHITE);
            sfx_play(SFX_ERROR);
            msg_timer = 90;
            break;
        case SUBMIT_NOT_IN_LIST:
            show_message(L->msg_not_in_list, PAL_TXT_WHITE);
            sfx_play(SFX_ERROR);
            msg_timer = 90;
            break;
        default: {
            int row = game.n_guesses - 1;
            msg_timer = 0;
            reveal_row(row);
            render_keyboard(&game, L);
            if (game.mode == MODE_MARATHON) {
                if (marathon_after_guess()) return SCR_MARATHON_RESULT;
            } else if (game.mode == MODE_TIME_ATTACK) {
                if (time_attack_after_guess()) return SCR_TA_RESULT;
            } else if (game.status != STATUS_PLAYING) {
                finish_game();
                return SCR_RESULT;
            }
            break;
        }
        }
    }
}

// ---------------------------------------------------------------------------
// Results
// ---------------------------------------------------------------------------

static void draw_stats(const Language *L, int ty, int highlight_row)
{
    txt_puts(2, ty, L->stats_played, PAL_TXT_GRAY);
    txt_uint(3 + strlen(L->stats_played), ty, save.played, PAL_TXT_WHITE);
    txt_puts(16, ty, L->stats_win_rate, PAL_TXT_GRAY);
    int x = 17 + strlen(L->stats_win_rate);
    unsigned rate = save.played ? (save.won * 100u) / save.played : 0;
    x += txt_uint(x, ty, rate, PAL_TXT_WHITE);
    txt_puts(x, ty, "%", PAL_TXT_WHITE);

    ty++;
    txt_puts(2, ty, L->stats_streak, PAL_TXT_GRAY);
    txt_uint(3 + strlen(L->stats_streak), ty, save.streak, PAL_TXT_WHITE);
    txt_puts(16, ty, L->stats_max_streak, PAL_TXT_GRAY);
    txt_uint(17 + strlen(L->stats_max_streak), ty, save.max_streak, PAL_TXT_WHITE);

    ty += 2;
    txt_puts(2, ty, L->stats_distribution, PAL_TXT_GRAY);
    unsigned max = 1;
    for (int i = 0; i < MAX_GUESSES; i++)
        if (save.dist[i] > max) max = save.dist[i];
    for (int i = 0; i < MAX_GUESSES; i++) {
        ty++;
        txt_uint(2, ty, i + 1, PAL_TXT_WHITE);
        int w = save.dist[i] ? 1 + (save.dist[i] * 19) / max : 0;
        if (w > 20) w = 20;
        int pal = i == highlight_row ? PAL_TXT_GREEN : PAL_TXT_DIM;
        txt_fill(4, ty, w, '\x03', pal);
        txt_uint(5 + w, ty, save.dist[i], PAL_TXT_WHITE);
    }

    ty += 2;
    txt_puts(2, ty, L->stats_classic, PAL_TXT_GRAY);
    txt_uint(3 + strlen(L->stats_classic), ty, save.classic_won, PAL_TXT_WHITE);
}

static Screen stats_screen(void)
{
    const Language *L = MLANG();
    render_clear();
    txt_center(1, L->stats_title, PAL_TXT_YELLOW);
    draw_stats(L, 4, -1);
    txt_center(18, L->stats_back, PAL_TXT_DIM);
    for (;;) {
        next_frame();
        if (input_hit(KEY_B | KEY_A | KEY_START)) { sfx_play(SFX_DELETE); return SCR_MENU; }
    }
}

static Screen result_screen(void)
{
    const Language *L = LANG();
    bool won = game.status == STATUS_WON;
    render_clear();

    if (won) {
        txt_center(1, L->win_msgs[game.n_guesses - 1], PAL_TXT_GREEN);
    } else {
        int n = strlen(L->lose_msg);
        int x = (SCREEN_TW - n - WORD_LEN) / 2;
        txt_puts(x, 1, L->lose_msg, PAL_TXT_YELLOW);
        char w[WORD_LEN + 1];
        memcpy(w, game.target, WORD_LEN);
        w[WORD_LEN] = 0;
        txt_puts(x + n, 1, w, PAL_TXT_WHITE);
    }
    draw_stats(L, 4, won ? game.n_guesses - 1 : -1);
    txt_center(18, L->result_prompt, PAL_TXT_DIM);

    for (;;) {
        next_frame();
        if (input_hit(KEY_A | KEY_START)) { sfx_play(SFX_SELECT); return SCR_GAME; }   // same mode again
        if (input_hit(KEY_B)) { sfx_play(SFX_SELECT); return SCR_MENU; }
    }
}

static Screen marathon_result_screen(void)
{
    const Language *L = LANG();
    render_clear();
    txt_center(1, L->marathon_over, PAL_TXT_YELLOW);
    draw_logo(4);
    txt_center(8, L->difficulty[marathon.difficulty], PAL_TXT_GRAY);

    int x = (SCREEN_TW - strlen(L->score) - 4) / 2;
    txt_puts(x, 10, L->score, PAL_TXT_GRAY);
    txt_uint(x + 1 + strlen(L->score), 10, marathon.score, PAL_TXT_WHITE);

    if (marathon.new_record) {
        txt_center(12, L->new_record, PAL_TXT_GREEN);
    } else {
        x = (SCREEN_TW - strlen(L->best) - 4) / 2;
        txt_puts(x, 12, L->best, PAL_TXT_GRAY);
        txt_uint(x + 1 + strlen(L->best), 12, save.marathon_best[marathon.difficulty], PAL_TXT_WHITE);
    }
    txt_center(18, L->result_prompt, PAL_TXT_DIM);

    for (;;) {
        next_frame();
        if (input_hit(KEY_A | KEY_START)) { sfx_play(SFX_SELECT); return SCR_GAME; }
        if (input_hit(KEY_B)) { sfx_play(SFX_SELECT); return SCR_MENU; }
    }
}

// Arcade-style initials entry: three letter cells, UP/DOWN change the letter,
// LEFT/RIGHT/A move, START (or A on the last letter) confirms.
static void enter_initials(char initials[3])
{
    const int ty = 12, tx0 = 12;
    int pos = 0;
    memcpy(initials, save.initials, 3);

    for (int i = 0; i < 3; i++) cell_draw(tx0 + i * 2, ty, initials[i], PAL_CELL_TYPED);
    for (;;) {
        cursor_set((tx0 + pos * 2) * 8, ty * 8, true);
        next_frame();
        bool changed = false;
        if (input_nav(KEY_UP))   { initials[pos] = initials[pos] == 'Z' ? 'A' : initials[pos] + 1; changed = true; }
        if (input_nav(KEY_DOWN)) { initials[pos] = initials[pos] == 'A' ? 'Z' : initials[pos] - 1; changed = true; }
        if (changed) {
            sfx_play(SFX_MOVE);
            cell_draw(tx0 + pos * 2, ty, initials[pos], PAL_CELL_TYPED);
        }
        if (input_hit(KEY_LEFT) && pos > 0)  { pos--; sfx_play(SFX_DELETE); }
        if (input_hit(KEY_RIGHT) && pos < 2) { pos++; sfx_play(SFX_KEY); }
        if (input_hit(KEY_B) && pos > 0)     { pos--; sfx_play(SFX_DELETE); }
        if (input_hit(KEY_A)) {
            if (pos < 2) { pos++; sfx_play(SFX_KEY); }
            else break;
        }
        if (input_hit(KEY_START)) break;
    }
    sfx_play(SFX_SELECT);
    cursor_set(0, 0, false);
    for (int i = 0; i < 3; i++) cell_draw(tx0 + i * 2, ty, initials[i], PAL_CORRECT);
    memcpy(save.initials, initials, 3);
}

static Screen ta_result_screen(void)
{
    const Language *L = LANG();
    render_clear();
    txt_center(1, L->ta_done, PAL_TXT_YELLOW);

    int x = (SCREEN_TW - 2 - 1 - strlen(L->words)) / 2;
    x += txt_uint(x, 3, ta.total, PAL_TXT_GRAY);
    txt_puts(x + 1, 3, L->words, PAL_TXT_GRAY);

    x = (SCREEN_TW - strlen(L->time) - 1 - 8) / 2;
    txt_puts(x, 5, L->time, PAL_TXT_GRAY);
    TimeRecord run = { .frames = ta.frames, .used = 1 };
    draw_time(x + 1 + strlen(L->time), 5, &run, PAL_TXT_WHITE);
    if (ta.missed) {                            // "PENALTIES 2 X 30S"
        x = (SCREEN_TW - strlen(L->penalties) - 9) / 2;
        txt_puts(x, 7, L->penalties, PAL_TXT_GRAY);
        x += strlen(L->penalties) + 1;
        x += txt_uint(x, 7, ta.missed, PAL_TXT_WHITE);
        txt_puts(x + 1, 7, "X 30S", PAL_TXT_DIM);
    }

    TimeRecord *board = save.ta_board[ta.length_idx];
    int highlight = -1;
    if (ta.rank >= 0) {
        txt_center(9, L->new_record, PAL_TXT_GREEN);
        txt_center(10, L->enter_initials, PAL_TXT_GRAY);
        txt_center(18, L->initials_help, PAL_TXT_DIM);
        char initials[3];
        enter_initials(initials);
        highlight = ta_board_insert(board, ta.frames, initials);
        stats_save();
        wait_frames(30);
        for (int r = 9; r <= 14; r++) txt_clear_row(r);
        cells_clear();
    }
    draw_leaderboard(9, board, highlight);
    txt_clear_row(18);
    txt_center(18, L->result_prompt, PAL_TXT_DIM);

    for (;;) {
        next_frame();
        if (input_hit(KEY_A | KEY_START)) { sfx_play(SFX_SELECT); return SCR_GAME; }
        if (input_hit(KEY_B)) { sfx_play(SFX_SELECT); return SCR_MENU; }
    }
}

// ---------------------------------------------------------------------------

int main(void)
{
    irq_init(NULL);
    irq_enable(II_VBLANK);

    stats_load();
    menu_lang = save.lang;
    render_init();
    sound_init();
    sound_set_enabled(save.sound_on);

    Screen scr = SCR_LANG;
    for (;;) {
        current_screen = scr;
        switch (scr) {
        case SCR_TITLE:           scr = title_screen();           break;
        case SCR_LANG:            scr = lang_screen();            break;
        case SCR_MENU:            scr = menu_screen();            break;
        case SCR_OPTIONS:         scr = options_screen();         break;
        case SCR_RECORDS:         scr = records_screen();         break;
        case SCR_MODE_SELECT:     scr = mode_select_screen();     break;
        case SCR_GAME:            scr = game_screen();            break;
        case SCR_RESULT:          scr = result_screen();          break;
        case SCR_MARATHON_RESULT: scr = marathon_result_screen(); break;
        case SCR_TA_RESULT:       scr = ta_result_screen();       break;
        case SCR_STATS:           scr = stats_screen();           break;
        }
    }
    return 0;
}
