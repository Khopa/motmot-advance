// Time Attack helpers: time formatting and the leaderboards.
#include "test.h"
#include "time_attack.h"

static void fmt(u32 frames, const char *expect)
{
    char buf[9];
    ta_format_time(frames, buf);
    CHECK_MEM(buf, expect, 9);
}

TEST(format_zero)              { fmt(0, "00:00.00"); }
TEST(format_one_second)        { fmt(60, "00:01.00"); }
TEST(format_half_second)       { fmt(30, "00:00.50"); }
TEST(format_one_frame)         { fmt(1, "00:00.01"); }
TEST(format_fifty_nine_frames) { fmt(59, "00:00.98"); }
TEST(format_minutes)           { fmt(60 * 60 * 2 + 60 * 31 + 12, "02:31.20"); }
TEST(format_never_overflows)   { fmt(0xFFFFFFFFu, "99:59.98"); }
TEST(format_always_terminated)
{
    char buf[9];
    memset(buf, 'X', sizeof buf);
    ta_format_time(12345, buf);
    CHECK_EQ(buf[8], 0);
    CHECK_EQ(buf[2], ':');
    CHECK_EQ(buf[5], '.');
}

TEST(word_counts)
{
    CHECK_EQ(ta_word_counts[0], 5);
    CHECK_EQ(ta_word_counts[1], 10);
    CHECK_EQ(ta_word_counts[2], 15);
    CHECK_EQ(TA_PENALTY_FRAMES, 30 * 60);
}

TEST(empty_board_ranks_first)
{
    TimeRecord board[TA_TOP] = { 0 };
    CHECK_EQ(ta_board_rank(board, 100000), 0);
}

TEST(insert_keeps_the_board_sorted)
{
    TimeRecord board[TA_TOP] = { 0 };
    CHECK_EQ(ta_board_insert(board, 3000, "BBB"), 0);
    CHECK_EQ(ta_board_insert(board, 5000, "CCC"), 1);
    CHECK_EQ(ta_board_insert(board, 1000, "AAA"), 0);     // new best shifts the others down
    CHECK_MEM(board[0].initials, "AAA", 3);
    CHECK_EQ(board[0].frames, 1000);
    CHECK_MEM(board[1].initials, "BBB", 3);
    CHECK_MEM(board[2].initials, "CCC", 3);
    CHECK_EQ(board[2].used, 1);
}

TEST(insert_in_the_middle)
{
    TimeRecord board[TA_TOP] = { 0 };
    ta_board_insert(board, 1000, "AAA");
    ta_board_insert(board, 3000, "CCC");
    CHECK_EQ(ta_board_insert(board, 2000, "BBB"), 1);
    CHECK_MEM(board[1].initials, "BBB", 3);
    CHECK_MEM(board[2].initials, "CCC", 3);
}

TEST(slower_than_a_full_board_is_rejected)
{
    TimeRecord board[TA_TOP] = { 0 };
    ta_board_insert(board, 1000, "AAA");
    ta_board_insert(board, 2000, "BBB");
    ta_board_insert(board, 3000, "CCC");
    CHECK_EQ(ta_board_rank(board, 3000), -1);              // a tie does not beat the record
    CHECK_EQ(ta_board_rank(board, 4000), -1);
    CHECK_EQ(ta_board_insert(board, 4000, "DDD"), -1);
    CHECK_MEM(board[2].initials, "CCC", 3);
}

TEST(last_place_drops_off)
{
    TimeRecord board[TA_TOP] = { 0 };
    ta_board_insert(board, 1000, "AAA");
    ta_board_insert(board, 2000, "BBB");
    ta_board_insert(board, 3000, "CCC");
    CHECK_EQ(ta_board_insert(board, 2500, "DDD"), 2);
    CHECK_MEM(board[2].initials, "DDD", 3);
    CHECK_EQ(board[2].frames, 2500);
    CHECK_EQ(ta_board_rank(board, 2999), -1);
}

TEST(rank_does_not_modify_the_board)
{
    TimeRecord board[TA_TOP] = { 0 };
    ta_board_insert(board, 1000, "AAA");
    CHECK_EQ(ta_board_rank(board, 500), 0);
    CHECK_EQ(ta_board_rank(board, 1500), 1);
    CHECK_EQ(board[1].used, 0);
    CHECK_MEM(board[0].initials, "AAA", 3);
}

static const TestCase time_attack_tests[] = {
    T(format_zero), T(format_one_second), T(format_half_second), T(format_one_frame),
    T(format_fifty_nine_frames), T(format_minutes), T(format_never_overflows), T(format_always_terminated),
    T(word_counts), T(empty_board_ranks_first), T(insert_keeps_the_board_sorted), T(insert_in_the_middle),
    T(slower_than_a_full_board_is_rejected), T(last_place_drops_off), T(rank_does_not_modify_the_board),
};
SUITE(time_attack)
