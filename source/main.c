// KhopaMotus — entry point and screen state machine:
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

typedef enum { SCR_TITLE, SCR_LANG, SCR_MENU, SCR_GAME, SCR_RESULT, SCR_STATS } Screen;

static GameState game;
static KbCursor  kb;
static u32       frames;            // since power-on; entropy for the RNG
static u8        menu_lang;         // language shown/selected in the menu
static int       menu_item;
static bool      challenge_resumed;

enum { MENU_LANG, MENU_CLASSIC, MENU_CHALLENGE, MENU_STATS, MENU_SOUND, MENU_COUNT };

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

// ---------------------------------------------------------------------------
// Title
// ---------------------------------------------------------------------------

static void draw_logo(int ty)
{
    static const char logo[] = "KHOPAMOTUS";
    static const u8 pals[] = { PAL_ABSENT, PAL_ABSENT, PAL_ABSENT, PAL_ABSENT, PAL_ABSENT,
                               PAL_CORRECT, PAL_PRESENT, PAL_CORRECT, PAL_PRESENT, PAL_CORRECT };
    for (int i = 0; i < 10; i++)
        cell_draw(5 + i * 2, ty, logo[i], pals[i]);
}

static Screen title_screen(void)
{
    const Language *L = &languages[menu_lang];
    render_clear();
    decor_show(true);
    draw_logo(5);
    txt_center(8, "GAME BOY ADVANCE", PAL_TXT_GRAY);
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
    static const int rows[LANG_COUNT] = { 9, 11 };
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
    txt_center(5, "CHOISIR LA LANGUE", PAL_TXT_GRAY);
    txt_center(6, "CHOOSE LANGUAGE", PAL_TXT_GRAY);
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

static void draw_menu(void)
{
    const Language *L = &languages[menu_lang];
    static const int rows[MENU_COUNT] = { 5, 8, 10, 12, 14 };

    txt_clear();
    cells_clear();
    draw_logo(1);

    txt_puts(5, rows[MENU_LANG], L->menu_language, PAL_TXT_WHITE);
    txt_puts(16, rows[MENU_LANG], "<", PAL_TXT_GRAY);
    txt_puts(18, rows[MENU_LANG], L->name, PAL_TXT_YELLOW);
    txt_puts(27, rows[MENU_LANG], ">", PAL_TXT_GRAY);

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

    txt_puts(5, rows[MENU_STATS], L->menu_stats, PAL_TXT_WHITE);

    txt_puts(5, rows[MENU_SOUND], L->menu_sound, PAL_TXT_WHITE);
    txt_puts(16, rows[MENU_SOUND], "<", PAL_TXT_GRAY);
    txt_puts(18, rows[MENU_SOUND], save.sound_on ? L->on : L->off, PAL_TXT_YELLOW);
    txt_puts(27, rows[MENU_SOUND], ">", PAL_TXT_GRAY);

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

        // left/right (or A) change the value of the highlighted option
        bool toggle = input_hit(KEY_LEFT | KEY_RIGHT) || input_hit(KEY_A);
        if (toggle && menu_item == MENU_LANG) {
            menu_lang = (menu_lang + 1) % LANG_COUNT;
            save.lang = menu_lang;
            stats_save();
            sfx_play(SFX_MOVE);
            dirty = true;
        } else if (toggle && menu_item == MENU_SOUND) {
            save.sound_on = !save.sound_on;
            sound_set_enabled(save.sound_on);
            stats_save();
            sfx_play(SFX_SELECT);
            dirty = true;
        }

        if (input_hit(KEY_A) || input_hit(KEY_START)) {
            switch (menu_item) {
            case MENU_CLASSIC:   sfx_play(SFX_SELECT); game.mode = MODE_CLASSIC;   return SCR_GAME;
            case MENU_CHALLENGE: sfx_play(SFX_SELECT); game.mode = MODE_CHALLENGE; return SCR_GAME;
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

static void start_game(u8 lang_id, u8 mode)
{
    challenge_resumed = false;

    if (mode == MODE_CLASSIC) {
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
        game_init(&game, lang_id, MODE_CLASSIC, L->solutions[idx]);
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

static void draw_mode_label(void)
{
    const Language *L = LANG();
    txt_clear_row(MSG_TY);
    if (game.mode == MODE_CLASSIC) {
        txt_center(MSG_TY, L->mode_classic, PAL_TXT_DIM);
    } else {
        int len = strlen(L->mode_challenge);
        int x = (SCREEN_TW - len - (game.challenge_no >= 100 ? 3 : game.challenge_no >= 10 ? 2 : 1)) / 2;
        txt_puts(x, MSG_TY, L->mode_challenge, PAL_TXT_DIM);
        txt_uint(x + len, MSG_TY, game.challenge_no, PAL_TXT_DIM);
    }
}

static void show_message(const char *msg, int pal)
{
    txt_clear_row(MSG_TY);
    txt_center(MSG_TY, msg, pal);
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
        char buf[32];
        int n = strlen(L->lose_msg);
        memcpy(buf, L->lose_msg, n);
        memcpy(buf + n, game.target, WORD_LEN);
        buf[n + WORD_LEN] = 0;
        show_message(buf, PAL_TXT_YELLOW);
    }
    cursor_set(0, 0, false);

    // let the player look at the board, then move on
    for (int i = 0; i < 150; i++) {
        next_frame();
        if (input_hit(KEY_A | KEY_B | KEY_START)) break;
    }
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

        if (input_hit(KEY_SELECT)) return SCR_MENU;   // Challenge progress is already saved

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
            reveal_row(row);
            render_keyboard(&game, L);
            if (game.status != STATUS_PLAYING) {
                finish_game();
                return SCR_RESULT;
            }
            break;
        }
        }
    }
}

// ---------------------------------------------------------------------------
// Statistics / result
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
        case SCR_TITLE:  scr = title_screen();  break;
        case SCR_LANG:   scr = lang_screen();   break;
        case SCR_MENU:   scr = menu_screen();   break;
        case SCR_GAME:   scr = game_screen();   break;
        case SCR_RESULT: scr = result_screen(); break;
        case SCR_STATS:  scr = stats_screen();  break;
        }
    }
    return 0;
}
