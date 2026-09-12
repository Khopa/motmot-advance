// Minimal unit-test framework: a suite is an array of named test functions,
// checks record failures with file/line, main.c prints a summary.
//
//   TEST(scoring_exact_match) { CHECK_EQ(logic_score(...), ...); }
//   const TestCase logic_tests[] = { T(scoring_exact_match), ... };
//   SUITE(logic)   -> defines run_logic(void)
#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

typedef void (*TestFn)(void);
typedef struct {
    const char *name;
    TestFn fn;
} TestCase;

extern int  test_checks, test_failures, test_verbose;
extern const char *test_filter;

void test_fail(const char *file, int line, const char *msg);
void test_run_suite(const char *suite, const TestCase *cases, int count);

#define TEST(name) static void name(void)
#define T(name) { #name, name }
#define SUITE(name) \
    void run_##name(void) { test_run_suite(#name, name##_tests, (int)(sizeof name##_tests / sizeof name##_tests[0])); }

#define CHECK(cond) do { \
    test_checks++; \
    if (!(cond)) test_fail(__FILE__, __LINE__, #cond); \
} while (0)

#define CHECK_EQ(actual, expected) do { \
    long long _a = (long long)(actual), _e = (long long)(expected); \
    test_checks++; \
    if (_a != _e) { \
        char _buf[256]; \
        snprintf(_buf, sizeof _buf, "%s == %s  (got %lld, expected %lld)", #actual, #expected, _a, _e); \
        test_fail(__FILE__, __LINE__, _buf); \
    } \
} while (0)

// compare n bytes (words are stored without terminator)
#define CHECK_MEM(actual, expected, n) do { \
    test_checks++; \
    if (memcmp((actual), (expected), (n)) != 0) { \
        char _buf[256]; \
        snprintf(_buf, sizeof _buf, "%s == \"%.*s\"  (got \"%.*s\")", #actual, (int)(n), (const char *)(expected), (int)(n), (const char *)(actual)); \
        test_fail(__FILE__, __LINE__, _buf); \
    } \
} while (0)

#endif
