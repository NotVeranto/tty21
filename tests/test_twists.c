#include "test.h"
#include "twists.h"
#include "game.h"
#include "rng.h"

#include <string.h>

static void setupTwistGame(Game *game, int floor, DealerId dealer,
                           TwistId previous) {
    memset(game, 0, sizeof(*game));
    game->run.floor = floor;
    game->run.dealerId = dealer;
    game->run.previousTwistId = previous;
}

TEST(twist_early_floors_have_none) {
    Game game;

    setupTwistGame(&game, 1, DEALER_CROUPIER, TWIST_NONE);
    ASSERT_EQ_INT(TWIST_NONE, selectTwistForFloor(&game));
    game.run.floor = 2;
    ASSERT_EQ_INT(TWIST_NONE, selectTwistForFloor(&game));
    game.run.floor = 3;
    ASSERT_EQ_INT(TWIST_NONE, selectTwistForFloor(&game));
}

TEST(twist_floor_four_has_valid_category) {
    const TwistInfo *info;
    TwistId id;

    rngInit(201U);
    {
        Game game;
        setupTwistGame(&game, 4, DEALER_CROUPIER, TWIST_NONE);
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

    rngInit(208U);
    {
        Game game;
        setupTwistGame(&game, 8, DEALER_CROUPIER, TWIST_NONE);
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
    Game game;

    rngInit(204U);
    previous = TWIST_LUCKY_DRAW;
    setupTwistGame(&game, 4, DEALER_CROUPIER, previous);
    distinct = 0;

    for (sampleIndex = 0; sampleIndex < 100; sampleIndex++) {
        game.run.previousTwistId = previous;
        id = selectTwistForFloor(&game);
        ASSERT_TRUE(id > TWIST_NONE && id < TWIST_COUNT);
        ASSERT_TRUE(id != previous);

        if (!seen[id]) {
            seen[id] = 1;
            distinct++;
        }
    }

    ASSERT_TRUE(distinct > 1);
}

TEST(twist_forced_hostile_floor_schedule) {
    Game game;
    TwistId id;

    rngInit(210U);
    setupTwistGame(&game, 10, DEALER_CROUPIER, TWIST_NONE);
    ASSERT_EQ_INT(1, twistIsForcedHostile(&game));
    id = selectTwistForFloor(&game);
    ASSERT_EQ_INT(TWIST_CAT_HOSTILE, getTwistInfo(id)->category);

    game.run.floor = 11;
    ASSERT_EQ_INT(0, twistIsForcedHostile(&game));
    game.run.floor = 12;
    ASSERT_EQ_INT(0, twistIsForcedHostile(&game));

    game.run.floor = 13;
    ASSERT_EQ_INT(1, twistIsForcedHostile(&game));
    id = selectTwistForFloor(&game);
    ASSERT_EQ_INT(TWIST_CAT_HOSTILE, getTwistInfo(id)->category);

    game.run.floor = 16;
    ASSERT_EQ_INT(1, twistIsForcedHostile(&game));
}

TEST(twist_boss_floors_are_not_forced_hostile) {
    Game game;

    setupTwistGame(&game, 10, DEALER_BOSS_PRIME, TWIST_NONE);
    ASSERT_EQ_INT(0, twistIsForcedHostile(&game));
    setupTwistGame(&game, 13, DEALER_BOSS_VOID, TWIST_NONE);
    ASSERT_EQ_INT(0, twistIsForcedHostile(&game));
}

TEST(twist_sixth_sense_does_not_override_forced_hostile) {
    Game game;
    TwistId id;

    setupTwistGame(&game, 10, DEALER_CROUPIER, TWIST_NONE);
    game.player.perkOwned[PERK_SIXTH_SENSE] = 1;
    ASSERT_EQ_INT(1, twistIsForcedHostile(&game));
    rngInit(216U);
    id = selectTwistForFloor(&game);
    ASSERT_EQ_INT(TWIST_CAT_HOSTILE, getTwistInfo(id)->category);
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
    RUN_TEST(twist_forced_hostile_floor_schedule);
    RUN_TEST(twist_boss_floors_are_not_forced_hostile);
    RUN_TEST(twist_sixth_sense_does_not_override_forced_hostile);
    RUN_TEST(twist_none_has_expected_name);
    RUN_TEST(twist_invalid_ids_fall_back_to_none);
    RUN_TEST(twist_spot_check_categories);
}
