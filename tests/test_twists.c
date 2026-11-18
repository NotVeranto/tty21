#include "test.h"
#include "twists.h"
#include "game.h"
#include "rng.h"

#include <string.h>

TEST(twist_early_floors_have_none) {
    Game game = {0};
    game.run.floor = 1; ASSERT_EQ_INT(TWIST_NONE, selectTwistForFloor(&game));
    game.run.floor = 2; ASSERT_EQ_INT(TWIST_NONE, selectTwistForFloor(&game));
    game.run.floor = 3; ASSERT_EQ_INT(TWIST_NONE, selectTwistForFloor(&game));
}

TEST(twist_floor_four_has_valid_category) {
    const TwistInfo *info;
    TwistId id;

    rngInit(201U, 1);
    {
        Game game = {0}; game.run.floor = 4; game.run.previousTwistId = TWIST_NONE;
        id = selectTwistForFloor(&game);
    }
    ASSERT_TRUE(id > TWIST_NONE && id < TWIST_COUNT);

    info = getTwistInfo(id);
    ASSERT_TRUE(info->category >= TWIST_CAT_FAVORABLE);
    ASSERT_TRUE(info->category <= TWIST_CAT_HOSTILE);
}

TEST(twist_floor_eight_has_valid_category) {
    const TwistInfo *info;
    TwistId id;

    rngInit(208U, 1);
    {
        Game game = {0}; game.run.floor = 8; game.run.previousTwistId = TWIST_NONE;
        id = selectTwistForFloor(&game);
    }
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

    rngInit(204U, 1);
    previous = TWIST_LUCKY_DRAW;
    distinct = 0;

    for (sampleIndex = 0; sampleIndex < 100; sampleIndex++) {
        {
            Game game = {0}; game.run.floor = 4; game.run.previousTwistId = previous;
            id = selectTwistForFloor(&game);
        }
        ASSERT_TRUE(id > TWIST_NONE && id < TWIST_COUNT);
        ASSERT_TRUE(id != previous);

        if (!seen[id]) {
            seen[id] = 1;
            distinct++;
        }
    }

    ASSERT_TRUE(distinct > 1);
}

TEST(twist_forced_hostile_schedule) {
    Game game = {0};
    game.run.dealerId = DEALER_ACCOUNTANT; game.run.floor = 10;
    ASSERT_EQ_INT(1, twistIsForcedHostile(&game));
    ASSERT_EQ_INT(TWIST_CAT_HOSTILE, getTwistInfo(selectTwistForFloor(&game))->category);
    game.run.floor = 11; ASSERT_EQ_INT(0, twistIsForcedHostile(&game));
    game.run.floor = 12; ASSERT_EQ_INT(0, twistIsForcedHostile(&game));
    game.run.floor = 13; ASSERT_EQ_INT(1, twistIsForcedHostile(&game));
    game.run.floor = 16; ASSERT_EQ_INT(1, twistIsForcedHostile(&game));
}

TEST(twist_boss_floors_are_not_forced_hostile) {
    Game game = {0}; game.run.dealerId = DEALER_BOSS_PRIME;
    game.run.floor = 10; ASSERT_EQ_INT(0, twistIsForcedHostile(&game));
    game.run.floor = 13; ASSERT_EQ_INT(0, twistIsForcedHostile(&game));
}

TEST(twist_sixth_sense_does_not_cancel_forced_hostility) {
    Game game = {0}; game.run.floor = 10; game.run.dealerId = DEALER_ACCOUNTANT;
    game.player.perkOwned[PERK_SIXTH_SENSE] = 1;
    ASSERT_EQ_INT(1, twistIsForcedHostile(&game));
    ASSERT_EQ_INT(TWIST_CAT_HOSTILE, getTwistInfo(selectTwistForFloor(&game))->category);
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


TEST(twist_sixth_sense_excludes_hostile_random_twists) {
    unsigned int seed;
    int floor;

    for (floor = 4; floor <= 12; floor += 4) {
        for (seed = 1; seed <= 100; seed++) {
            Game game = {0};
            TwistId id;

            game.run.floor = floor;
            game.run.previousTwistId = TWIST_NONE;
            game.run.dealerId = DEALER_ACCOUNTANT;
            game.player.perkOwned[PERK_SIXTH_SENSE] = 1;
            rngInit(seed, 1);
            id = selectTwistForFloor(&game);

            ASSERT_TRUE(id > TWIST_NONE && id < TWIST_COUNT);
            ASSERT_TRUE(getTwistInfo(id)->category != TWIST_CAT_HOSTILE);
        }
    }
}

TEST(twist_sixth_sense_keeps_forced_hostile_floors) {
    Game game = {0};
    TwistId id;

    game.run.floor = 10;
    game.run.dealerId = DEALER_ACCOUNTANT;
    game.player.perkOwned[PERK_SIXTH_SENSE] = 1;
    rngInit(991U, 1);
    id = selectTwistForFloor(&game);

    ASSERT_EQ_INT(TWIST_CAT_HOSTILE, getTwistInfo(id)->category);
}

void run_twists_tests(void) {
    RUN_TEST(twist_early_floors_have_none);
    RUN_TEST(twist_floor_four_has_valid_category);
    RUN_TEST(twist_floor_eight_has_valid_category);
    RUN_TEST(twist_selection_avoids_previous_and_varies);
    RUN_TEST(twist_forced_hostile_schedule);
    RUN_TEST(twist_boss_floors_are_not_forced_hostile);
    RUN_TEST(twist_sixth_sense_does_not_cancel_forced_hostility);
    RUN_TEST(twist_sixth_sense_excludes_hostile_random_twists);
    RUN_TEST(twist_sixth_sense_keeps_forced_hostile_floors);
    RUN_TEST(twist_none_has_expected_name);
    RUN_TEST(twist_invalid_ids_fall_back_to_none);
    RUN_TEST(twist_spot_check_categories);
}
