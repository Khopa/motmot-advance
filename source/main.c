// MotMot Advance — entry point and screen state machine:
//   language (once, at boot) -> title -> menu -> game -> result -> menu ...
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

typedef enum {
    SCR_TITLE, SCR_LANG, SCR_MENU, SCR_GAME, SCR_RESULT, SCR_MARATHON_RESULT, SCR_STATS
} Screen;

static GameState game;
static KbCursor  kb;
static u32       frames;            // since power-on; entropy for the RNG
static u8        menu_lang;         // language shown/selected in the menu
static int       menu_item;
static bool      challenge_resumed;

// Marathon run: chained random words, lives, high score per difficulty
static struct {
    u8   difficulty;                // DIFF_EASY / DIFF_HARD
    u8   hp, hp_max;
    u16  score;                     // words found
    bool new_record;
} marathon;

static const u8 marathon_hp[DIFF_COUNT] = { 3, 5 };

enum { MENU_LANG, MENU_CLASSIC, MENU_CHALLENGE, MENU_MARATHON, MENU_STATS, MENU_SOUND, MENU_COUNT };

#define LANG() (&languages[game.lang])

static void next_frame(void)
{
    VBlankIntrWait();
    render_vblank();
    sound_update();
    input_poll();
    frames++;
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
// Title
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

static Screen title_screen(void)
{
    const Language *L = &languages[menu_lang];
    render_clear();
    decor_show(true);
    draw_logo(5);
    txt_center(18, "KHOPA - 2026", PAL_TXT_GRAY);

    for (;;) {
        next_frame();
        if ((frames & 31) == 0) txt_center(13, L->press_start, PAL_TXT_WHITE);
        if ((frames & 31) == 20) txt_clear_row(13);
        if (input_hit(KEY_START | KEY_A)) { sfx_play(SFX_SELECT); return SCR_MENU; }
    }
}

// ---------------------------------------------------------------------------
// Language selection: shown once at boot (still changeable from the menu)
// ---------------------------------------------------------------------------

static void draw_lang_choice(void)
{
    static const int rows[LANG_COUNT] = { 10, 12 };
    for (int i = 0; i < LANG_COUNT; i++) {
        txt_puts(10, rows[i], "  ", PAL_TXT_WHITE);
        txt_puts(12, rows[i], languages[i].name, i == menu_lang ? PAL_TXT_YELLOW : PAL_TXT_WHITE);
    }
    txt_puts(10, rows[menu_lang], ">", PAL_TXT_GREEN);
}

static Screen lang_screen(void)
{
    render_clear();
    draw_logo(1);
    txt_center(6, "CHOISIR LA LANGUE", PAL_TXT_GRAY);
    txt_center(7, "CHOOSE LANGUAGE", PAL_TXT_GRAY);
    txt_center(18, "A: OK", PAL_TXT_DIM);
    draw_lang_choice();

    for (;;) {
        next_frame();
        if (input_nav(KEY_UP) || input_nav(KEY_DOWN) || input_nav(KEY_LEFT) || input_nav(KEY_RIGHT)) {
            menu_lang = (menu_lang + 1) % LANG_COUNT;   // two languages: any direction toggles
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
// Menu
// ---------------------------------------------------------------------------

static void draw_option_value(int ty, const char *value)
{
    txt_puts(16, ty, "<", PAL_TXT_GRAY);
    txt_puts(18, ty, value, PAL_TXT_YELLOW);
    txt_puts(27, ty, ">", PAL_TXT_GRAY);
}

static void draw_menu(void)
{
    const Language *L = &languages[menu_lang];
    static const int rows[MENU_COUNT] = { 5, 7, 9, 11, 13, 15 };

    txt_clear();
    cells_clear();
    draw_logo(1);

    txt_puts(5, rows[MENU_LANG], L->menu_language, PAL_TXT_WHITE);
    draw_option_value(rows[MENU_LANG], L->name);

    txt_puts(5, rows[MENU_CLASSIC], L->menu_classic, PAL_TXT_WHITE);

    // Challenge: number of the next (or in-progress) challenge
    u8 ch_lang = save.ch_active ? save.ch_lang : menu_lang;
    txt_puts(5, rows[MENU_CHALLENGE], L->menu_challenge, PAL_TXT_WHITE);
    int x = 5 + strlen(L->menu_challenge) + 1;
    txt_puts(x, rows[MENU_CHALLENGE], "#", PAL_TXT_GRAY);
    x += 1 + txt_uint(x + 1, rows[MENU_CHALLENGE], save.challenge_done[ch_lang] + 1, PAL_TXT_GRAY);
    if (save.ch_active) {
        txt_puts(x + 2, rows[MENU_CHALLENGE], languages[save.ch_lang].name, PAL_TXT_DIM);
        txt_puts(x + 2 + strlen(languages[save.ch_lang].name), rows[MENU_CHALLENGE], "...", PAL_TXT_DIM);
    }

    txt_puts(5, rows[MENU_MARATHON], L->menu_marathon, PAL_TXT_WHITE);
    draw_option_value(rows[MENU_MARATHON], L->difficulty[save.marathon_diff]);

    txt_puts(5, rows[MENU_STATS], L->menu_stats, PAL_TXT_WHITE);

    txt_puts(5, rows[MENU_SOUND], L->menu_sound, PAL_TXT_WHITE);
    draw_option_value(rows[MENU_SOUND], save.sound_on ? L->on : L->off);

    txt_center(18, L->menu_help, PAL_TXT_DIM);
    txt_puts(3, rows[menu_item], ">", PAL_TXT_GREEN);
}

static Screen menu_screen(void)
{
    render_clear();
    draw_menu();

    for (;;) {
        next_frame();
        bool dirty = false;

        if (input_nav(KEY_UP))   { menu_item = (menu_item + MENU_COUNT - 1) % MENU_COUNT; dirty = true; }
        if (input_nav(KEY_DOWN)) { menu_item = (menu_item + 1) % MENU_COUNT; dirty = true; }
        if (dirty) sfx_play(SFX_MOVE);

        // left/right change the value of the highlighted option (A too for
        // options that cannot be "started")
        bool lr = input_hit(KEY_LEFT | KEY_RIGHT);
        bool a  = input_hit(KEY_A) || input_hit(KEY_START);
        if ((lr || a) && menu_item == MENU_LANG) {
            menu_lang = (menu_lang + 1) % LANG_COUNT;
            save.lang = menu_lang;
            stats_save();
            sfx_play(SFX_MOVE);
            dirty = true;
        } else if ((lr || a) && menu_item == MENU_SOUND) {
            save.sound_on = !save.sound_on;
            sound_set_enabled(save.sound_on);
            stats_save();
            sfx_play(SFX_SELECT);
            dirty = true;
        } else if (lr && menu_item == MENU_MARATHON) {
            save.marathon_diff = (save.marathon_diff + 1) % DIFF_COUNT;
            stats_save();
            sfx_play(SFX_MOVE);
            dirty = true;
        } else if (a) {
            switch (menu_item) {
            case MENU_CLASSIC:   sfx_play(SFX_SELECT); game.mode = MODE_CLASSIC;   return SCR_GAME;
            case MENU_CHALLENGE: sfx_play(SFX_SELECT); game.mode = MODE_CHALLENGE; return SCR_GAME;
            case MENU_MARATHON:  sfx_play(SFX_SELECT); game.mode = MODE_MARATHON;  return SCR_GAME;
            case MENU_STATS:     sfx_play(SFX_SELECT); return SCR_STATS;
            default: break;
            }
        }
        if (input_hit(KEY_B)) return SCR_TITLE;
        if (dirty) draw_menu();
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
    challenge_resumed = false;

    if (mode == MODE_CLASSIC) {
        random_word(lang_id, MODE_CLASSIC);
        return;
    }
    if (mode == MODE_MARATHON) {
        marathon.difficulty = save.marathon_diff;
        marathon.hp_max = marathon.hp = marathon_hp[marathon.difficulty];
        marathon.score = 0;
        marathon.new_record = false;
        random_word(lang_id, MODE_MARATHON);
        return;
    }

    // Challenge: at most one in progress, in the language it was started in.
    if (save.ch_active) lang_id = save.ch_lang;
    const Language *L = &languages[lang_id];
    u16 n = save.challenge_done[lang_id];
    u16 idx = L->challenge_seq[n % L->n_solutions];
    game_init(&game, lang_id, MODE_CHALLENGE, L->solutions[idx]);
    game.challenge_no = n + 1;

    if (save.ch_active) {
        for (int i = 0; i < save.ch_n_guesses; i++)
            game_replay_guess(&game, save.ch_guesses[i]);
        challenge_resumed = save.ch_n_guesses > 0;
    } else {
        save.ch_active = 1;
        save.ch_lang = lang_id;
        save.ch_n_guesses = 0;
        stats_save();
    }
}

// Row 0: mode name, or "SCORE n" + hearts in Marathon
static void draw_mode_label(void)
{
    const Language *L = LANG();
    txt_clear_row(MSG_TY);
    if (game.mode == MODE_CLASSIC) {
        txt_center(MSG_TY, L->mode_classic, PAL_TXT_DIM);
    } else if (game.mode == MODE_CHALLENGE) {
        int len = strlen(L->mode_challenge);
        int x = (SCREEN_TW - len - (game.challenge_no >= 100 ? 3 : game.challenge_no >= 10 ? 2 : 1)) / 2;
        txt_puts(x, MSG_TY, L->mode_challenge, PAL_TXT_DIM);
        txt_uint(x + len, MSG_TY, game.challenge_no, PAL_TXT_DIM);
    } else {
        txt_puts(1, MSG_TY, L->marathon_score, PAL_TXT_GRAY);
        txt_uint(2 + strlen(L->marathon_score), MSG_TY, marathon.score, PAL_TXT_WHITE);
        int x = SCREEN_TW - 1 - marathon.hp_max;
        txt_fill(x, MSG_TY, marathon.hp, '\x04', PAL_TXT_YELLOW);
        txt_fill(x + marathon.hp, MSG_TY, marathon.hp_max - marathon.hp, '\x05', PAL_TXT_DIM);
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

// SELECT: ask before leaving the game. Returns true to quit.
static bool confirm_quit(void)
{
    const Language *L = LANG();
    show_message(L->quit_confirm, PAL_TXT_YELLOW);
    sfx_play(SFX_MOVE);
    for (;;) {
        next_frame();
        if (input_hit(KEY_A | KEY_START)) { sfx_play(SFX_SELECT); return true; }
        if (input_hit(KEY_B | KEY_SELECT)) { sfx_play(SFX_DELETE); draw_mode_label(); return false; }
    }
}

// Classic / Challenge: record the result, show the verdict, wait a bit.
static void finish_game(void)
{
    const Language *L = LANG();
    bool won = game.status == STATUS_WON;

    stats_record_result(won, game.n_guesses);
    if (game.mode == MODE_CHALLENGE) {
        save.ch_active = 0;
        save.challenge_done[game.lang]++;
        if (won) save.challenge_won++;
    }
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

// Record the run's score; returns true if it is a new best.
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

    wait_or_key(120);
    if (marathon.hp == 0) {
        marathon_finish();
        return true;
    }

    // next word: same language, fresh grid and keyboard
    random_word(game.lang, MODE_MARATHON);
    render_grid(&game);
    render_keyboard(&game, L);
    draw_mode_label();
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

    int msg_timer = 0;
    if (challenge_resumed) {
        show_message(L->challenge_resumed, PAL_TXT_YELLOW);
        msg_timer = 120;
    } else {
        draw_mode_label();
    }

    for (;;) {
        next_frame();

        if (msg_timer > 0 && --msg_timer == 0) draw_mode_label();

        if (input_hit(KEY_SELECT)) {
            msg_timer = 0;
            if (!confirm_quit()) continue;
            // Challenge progress is already saved; a Marathon run ends here
            if (game.mode == MODE_MARATHON) {
                marathon_finish();
                return SCR_MARATHON_RESULT;
            }
            return SCR_MENU;
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
            if (game.mode == MODE_CHALLENGE) {
                memcpy(save.ch_guesses[row], game.guesses[row], WORD_LEN);
                save.ch_n_guesses = game.n_guesses;
                stats_save();
            }
            msg_timer = 0;
            reveal_row(row);
            render_keyboard(&game, L);
            if (game.mode == MODE_MARATHON) {
                if (marathon_after_guess()) return SCR_MARATHON_RESULT;
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
// Statistics / results
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
    txt_puts(2, ty, L->stats_challenges, PAL_TXT_GRAY);
    txt_uint(3 + strlen(L->stats_challenges), ty, save.challenge_won, PAL_TXT_WHITE);

    // marathon best scores, one line per difficulty
    for (int d = 0; d < DIFF_COUNT; d++) {
        ty++;
        txt_puts(2, ty, L->menu_marathon, PAL_TXT_GRAY);
        x = 3 + strlen(L->menu_marathon);
        txt_puts(x, ty, L->difficulty[d], PAL_TXT_GRAY);
        txt_uint(x + 1 + strlen(L->difficulty[d]), ty, save.marathon_best[d], PAL_TXT_WHITE);
    }
}

static Screen stats_screen(void)
{
    const Language *L = &languages[menu_lang];
    render_clear();
    txt_center(1, L->stats_title, PAL_TXT_WHITE);
    draw_stats(L, 3, -1);
    txt_center(18, L->stats_back, PAL_TXT_DIM);
    for (;;) {
        next_frame();
        if (input_hit(KEY_B | KEY_A | KEY_START)) return SCR_MENU;
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
    draw_stats(L, 3, won ? game.n_guesses - 1 : -1);
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

    int x = (SCREEN_TW - strlen(L->marathon_score) - 4) / 2;
    txt_puts(x, 10, L->marathon_score, PAL_TXT_GRAY);
    txt_uint(x + 1 + strlen(L->marathon_score), 10, marathon.score, PAL_TXT_WHITE);

    if (marathon.new_record) {
        txt_center(12, L->new_record, PAL_TXT_GREEN);
    } else {
        x = (SCREEN_TW - strlen(L->marathon_best) - 4) / 2;
        txt_puts(x, 12, L->marathon_best, PAL_TXT_GRAY);
        txt_uint(x + 1 + strlen(L->marathon_best), 12, save.marathon_best[marathon.difficulty], PAL_TXT_WHITE);
    }
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
        switch (scr) {
        case SCR_TITLE:           scr = title_screen();           break;
        case SCR_LANG:            scr = lang_screen();            break;
        case SCR_MENU:            scr = menu_screen();            break;
        case SCR_GAME:            scr = game_screen();            break;
        case SCR_RESULT:          scr = result_screen();          break;
        case SCR_MARATHON_RESULT: scr = marathon_result_screen(); break;
        case SCR_STATS:           scr = stats_screen();           break;
        }
    }
    return 0;
}
