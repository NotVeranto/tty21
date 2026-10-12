#include "test.h"
#include "rng.h"

TEST(rng_range_zero_is_always_zero) {
    int sampleIndex;

    rngInit(101U);

    for (sampleIndex = 0; sampleIndex < 100; sampleIndex++) {
        ASSERT_EQ_INT(0, rngRange(0, 0));
    }
}

TEST(rng_range_single_value_is_inclusive) {
    int sampleIndex;

    rngInit(102U);

    for (sampleIndex = 0; sampleIndex < 100; sampleIndex++) {
        ASSERT_EQ_INT(5, rngRange(5, 5));
    }
}

TEST(rng_range_one_to_ten_bounds_and_diversity) {
    int seen[11] = {0};
    int sampleIndex;
    int value;
    int minimum;
    int maximum;
    int distinct;

    rngInit(103U);
    minimum = 11;
    maximum = 0;
    distinct = 0;

    for (sampleIndex = 0; sampleIndex < 1000; sampleIndex++) {
        value = rngRange(1, 10);
        ASSERT_TRUE(value >= 1 && value <= 10);

        if (value < minimum) {
            minimum = value;
        }
        if (value > maximum) {
            maximum = value;
        }
        if (!seen[value]) {
            seen[value] = 1;
            distinct++;
        }
    }

    ASSERT_TRUE(minimum >= 1);
    ASSERT_TRUE(maximum <= 10);
    ASSERT_TRUE(distinct >= 5);
}

TEST(rng_range_zero_to_ninety_nine_bounds_and_diversity) {
    int seen[100] = {0};
    int sampleIndex;
    int value;
    int minimum;
    int maximum;
    int distinct;

    rngInit(104U);
    minimum = 100;
    maximum = -1;
    distinct = 0;

    for (sampleIndex = 0; sampleIndex < 1000; sampleIndex++) {
        value = rngRange(0, 99);
        ASSERT_TRUE(value >= 0 && value <= 99);

        if (value < minimum) {
            minimum = value;
        }
        if (value > maximum) {
            maximum = value;
        }
        if (!seen[value]) {
            seen[value] = 1;
            distinct++;
        }
    }

    ASSERT_TRUE(minimum >= 0);
    ASSERT_TRUE(maximum <= 99);
    ASSERT_TRUE(distinct >= 5);
}

TEST(rng_same_seed_repeats_first_ten_draws) {
    unsigned int firstSequence[10];
    int sampleIndex;

    rngInit(123456U);
    for (sampleIndex = 0; sampleIndex < 10; sampleIndex++) {
        firstSequence[sampleIndex] = rngNext();
    }

    rngInit(123456U);
    for (sampleIndex = 0; sampleIndex < 10; sampleIndex++) {
        ASSERT_EQ_INT(firstSequence[sampleIndex], rngNext());
    }
}

void run_rng_tests(void) {
    RUN_TEST(rng_range_zero_is_always_zero);
    RUN_TEST(rng_range_single_value_is_inclusive);
    RUN_TEST(rng_range_one_to_ten_bounds_and_diversity);
    RUN_TEST(rng_range_zero_to_ninety_nine_bounds_and_diversity);
    RUN_TEST(rng_same_seed_repeats_first_ten_draws);
}
