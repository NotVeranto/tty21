#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <stdlib.h>

extern int tty21_tests_run;
extern int tty21_tests_failed;

#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        int failuresBefore; \
        tty21_tests_run++; \
        printf("  %-50s", #name); \
        fflush(stdout); \
        failuresBefore = tty21_tests_failed; \
        name(); \
        if (tty21_tests_failed == failuresBefore) { \
            printf("ok\n"); \
        } else { \
            printf("FAIL\n"); \
        } \
    } \
    static void name(void)

#define ASSERT_EQ_INT(expected, actual) \
    do { \
        long long _e = (long long)(expected); \
        long long _a = (long long)(actual); \
        if (_e != _a) { \
            printf("\n    FAIL %s:%d\n", __FILE__, __LINE__); \
            printf("      expected %lld, got %lld\n", _e, _a); \
            tty21_tests_failed++; \
            return; \
        } \
    } while (0)

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            printf("\n    FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            tty21_tests_failed++; \
            return; \
        } \
    } while (0)

#define RUN_TEST(name) run_##name()

#define TEST_SUMMARY() \
    do { \
        printf("\n%d run, %d failed\n", \
               tty21_tests_run, tty21_tests_failed); \
        return tty21_tests_failed == 0 ? 0 : 1; \
    } while (0)

#endif
