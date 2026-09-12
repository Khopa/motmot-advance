// xorshift32 generator.
#include "test.h"
#include "rng.h"

TEST(same_seed_same_sequence)
{
    u32 a[8], b[8];
    rng_seed(12345);
    for (int i = 0; i < 8; i++) a[i] = rng_next();
    rng_seed(12345);
    for (int i = 0; i < 8; i++) b[i] = rng_next();
    for (int i = 0; i < 8; i++) CHECK_EQ(a[i], b[i]);
}

TEST(different_seeds_differ)
{
    rng_seed(1);
    u32 a = rng_next();
    rng_seed(2);
    CHECK(rng_next() != a);
}

TEST(zero_seed_is_replaced)
{
    // xorshift would be stuck at 0 forever; seeding with 0 must still produce values
    rng_seed(0);
    CHECK(rng_next() != 0);
    CHECK(rng_next() != 0);
}

TEST(state_can_be_saved_and_restored)
{
    rng_seed(777);
    rng_next();
    u32 s = rng_state();
    u32 next = rng_next();
    rng_seed(s);
    CHECK_EQ(rng_next(), next);
}

TEST(range_stays_in_bounds)
{
    rng_seed(42);
    for (int i = 0; i < 10000; i++) {
        CHECK(rng_range(1) == 0);
        CHECK(rng_range(7) < 7);
        CHECK(rng_range(500) < 500);
    }
}

TEST(range_reaches_every_value)
{
    int seen[10] = { 0 };
    rng_seed(99);
    for (int i = 0; i < 2000; i++) seen[rng_range(10)]++;
    for (int i = 0; i < 10; i++) CHECK(seen[i] > 100);     // ~200 expected each
}

TEST(no_short_cycle)
{
    rng_seed(31337);
    u32 first = rng_next();
    for (int i = 0; i < 100000; i++) CHECK(rng_next() != first);
}

static const TestCase rng_tests[] = {
    T(same_seed_same_sequence), T(different_seeds_differ), T(zero_seed_is_replaced),
    T(state_can_be_saved_and_restored), T(range_stays_in_bounds), T(range_reaches_every_value),
    T(no_short_cycle),
};
SUITE(rng)
