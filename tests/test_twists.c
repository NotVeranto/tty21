#include "test.h"
#include "twists.h"
#include "rng.h"

#include <string.h>

TEST(twist_early_floors_have_none) {
    ASSERT_EQ_INT(TWIST_NONE, selectTwistForFloor(1, TWIST_NONE));
    ASSERT_EQ_INT(TWIST_NONE, selectTwistForFloor(2, TWIST_NONE));
    ASSERT_EQ_INT(TWIST_NONE, selectTwistForFloor(3, TWIST_NONE));
}

TEST(twist_floor_four_has_valid_category) {
    const TwistInfo *info;
    TwistId id;

    rngInit(201U);
    id = selectTwistForFloor(4, TWIST_NONE);
    ASSERT_TRUE(id > TWIST_NONE && id < TWIST_COUNT);

    info = getTwistInfo(id);
    ASSERT_TRUE(info->category >= TWIST_CAT_FAVORABLE);
    ASSERT_TRUE(info->category <= TWIST_CAT_HOSTILE);
}

TEST(twist_floor_eight_has_valid_category) {
    const TwistInfo *info;
    TwistId id;

    rngInit(208U);
    id = selectTwistForFloor(8, TWIST_NONE);
    ASSERT_TRUE(id > TWIST_NONE && id < TWIST_COUNT);

    info = getTwistInfo(id);
    ASSERT_TRUE(info->category >= TWIST_CAT_FAVORABLE);
    ASSERT_TRUE(info->category <= TWIST_CAT_HOSTILE);
}

TEST(twist_selection_avoids_previous_and_varies) {
    int seen[TWIST_COUNT] = {0};
    int sampleIndex;
    int distinct;
    TwistId id;
    TwistId previous;

    rngInit(204U);
    previous = TWIST_LUCKY_DRAW;
    distinct = 0;

    for (sampleIndex = 0; sampleIndex < 100; sampleIndex++) {
        id = selectTwistForFloor(4, previous);
        ASSERT_TRUE(id > TWIST_NONE && id < TWIST_COUNT);
        ASSERT_TRUE(id != previous);

        if (!seen[id]) {
            seen[id] = 1;
            distinct++;
        }
    }

    ASSERT_TRUE(distinct > 1);
}

TEST(twist_none_has_expected_name) {
    ASSERT_TRUE(getTwistInfo(TWIST_NONE)->name != NULL);
    ASSERT_TRUE(strcmp(getTwistInfo(TWIST_NONE)->name, "None") == 0);
}

TEST(twist_invalid_ids_fall_back_to_none) {
    const TwistInfo *negativeInfo;
    const TwistInfo *largeInfo;

    negativeInfo = getTwistInfo((TwistId)-1);
    largeInfo = getTwistInfo(TWIST_COUNT);

    ASSERT_EQ_INT(TWIST_NONE, negativeInfo->id);
    ASSERT_EQ_INT(TWIST_NONE, largeInfo->id);
    ASSERT_TRUE(strcmp(negativeInfo->name, "None") == 0);
    ASSERT_TRUE(strcmp(largeInfo->name, "None") == 0);
}

TEST(twist_spot_check_categories) {
    ASSERT_EQ_INT(TWIST_CAT_FAVORABLE,
                  getTwistInfo(TWIST_JACKPOT)->category);
    ASSERT_EQ_INT(TWIST_CAT_HOSTILE,
                  getTwistInfo(TWIST_DESPERATION)->category);
    ASSERT_EQ_INT(TWIST_CAT_NEUTRAL,
                  getTwistInfo(TWIST_ACES_LOW)->category);
}

void run_twists_tests(void) {
    RUN_TEST(twist_early_floors_have_none);
    RUN_TEST(twist_floor_four_has_valid_category);
    RUN_TEST(twist_floor_eight_has_valid_category);
    RUN_TEST(twist_selection_avoids_previous_and_varies);
    RUN_TEST(twist_none_has_expected_name);
    RUN_TEST(twist_invalid_ids_fall_back_to_none);
    RUN_TEST(twist_spot_check_categories);
}
