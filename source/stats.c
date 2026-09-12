// SRAM persistence. Cartridge SRAM (0x0E000000, 32 KiB) is on an 8-bit bus:
// it must be accessed one byte at a time, and it is safest to run that code
// from IWRAM rather than ROM, hence IWRAM_CODE on the copy loops.
#include <stddef.h>
#include <string.h>
#include "stats.h"

#define SAVE_MAGIC   0x4D544F4Du   // "MOTM"
#define SAVE_VERSION 6

// Tells emulators and flash carts which backup type the ROM expects.
const char save_type_id[] __attribute__((aligned(4), used)) = "SRAM_V113";

SaveData save;

#define SRAM ((volatile u8 *)MEM_SRAM)

IWRAM_CODE __attribute__((noinline)) static void sram_read(void *dst, u32 len)
{
    u8 *d = dst;
    for (u32 i = 0; i < len; i++)
        d[i] = SRAM[i];
}

IWRAM_CODE __attribute__((noinline)) static void sram_write(const void *src, u32 len)
{
    const u8 *s = src;
    for (u32 i = 0; i < len; i++)
        SRAM[i] = s[i];
}

static u16 checksum(const SaveData *s)
{
    const u8 *p = (const u8 *)s;
    u32 sum = 0;
    for (u32 i = 0; i < offsetof(SaveData, checksum); i++)
        sum += p[i];
    return (u16)(sum ^ (sum >> 16));
}

static void defaults(void)
{
    memset(&save, 0, sizeof save);
    save.magic = SAVE_MAGIC;
    save.version = SAVE_VERSION;
    save.lang = LANG_FR;
    save.sound_on = 1;
    memcpy(save.initials, "AAA", 3);
    save.rng_state = 0x2545F491u;
}

void stats_load(void)
{
    // 8 wait cycles for SRAM (bits 0-1 of WAITCNT), standard ROM timings
    REG_WAITCNT = WS_STANDARD | WS_SRAM_8;

    sram_read(&save, sizeof save);
    if (save.magic != SAVE_MAGIC || save.version != SAVE_VERSION
            || save.checksum != checksum(&save) || save.lang >= LANG_COUNT
            || save.marathon_diff >= DIFF_COUNT || save.sound_on > 1
            || save.ta_length >= TA_LENGTH_COUNT) {
        defaults();
        stats_save();
    }
}

void stats_save(void)
{
    save.checksum = checksum(&save);
    sram_write(&save, sizeof save);
}

void stats_record_result(bool won, int n_guesses)
{
    save.played++;
    if (won) {
        save.won++;
        save.streak++;
        if (save.streak > save.max_streak) save.max_streak = save.streak;
        if (n_guesses >= 1 && n_guesses <= MAX_GUESSES)
            save.dist[n_guesses - 1]++;
    } else {
        save.lost++;
        save.streak = 0;
    }
}

bool stats_recently_played(u8 lang, u16 solution_index)
{
    for (int i = 0; i < RECENT_WORDS; i++)
        if (save.recent[lang][i] == solution_index + 1)   // stored +1 so 0 = empty
            return true;
    return false;
}

void stats_push_recent(u8 lang, u16 solution_index)
{
    u8 *pos = &save.recent_pos[lang];
    save.recent[lang][*pos] = solution_index + 1;
    *pos = (*pos + 1) % RECENT_WORDS;
}
