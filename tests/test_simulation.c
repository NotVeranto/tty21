#include "test.h"
#include "game.h"
#include "round_simulation.h"
#include "round_economy.h"
#include "rng.h"

static ActionId stand_policy(const Game *game,
                             const ActionId *actions,
                             int actionCount,
                             void *context) {
    int index;
    (void)game;
    (void)context;
    for (index = 0; index < actionCount; index++) {
        if (actions[index] == ACTION_STAND) return ACTION_STAND;
    }
    return ACTION_COUNT;
}

static int run_seeded_stand_round(unsigned int seed,
                                  int *finalChips,
                                  MessageStyle *style) {
    Game game;

    rngInit(seed, 1);
    initGame(&game, MODE_CLASSIC);
    game.fastMode = 1;
    game.saveEnabled = 0;

    if (!simulateRound(&game, 10, 0, 0,
                       stand_policy, NULL, style)) {
        return 0;
    }
    if (game.roundNumber != 1 || game.roundInProgress ||
        game.state != STATE_BET || game.player.chips < 0 ||
        game.lastPayoutBreakdown[0] == '\0') {
        return 0;
    }
    *finalChips = game.player.chips;
    return 1;
}

TEST(policy_runner_completes_and_repeats_seeded_round) {
    int firstChips;
    int secondChips;
    MessageStyle firstStyle;
    MessageStyle secondStyle;

    ASSERT_TRUE(run_seeded_stand_round(20261119,
                                       &firstChips, &firstStyle));
    ASSERT_TRUE(run_seeded_stand_round(20261119,
                                       &secondChips, &secondStyle));
    ASSERT_EQ_INT(firstChips, secondChips);
    ASSERT_EQ_INT(firstStyle, secondStyle);
}

TEST(policy_runner_rejects_unavailable_policy_action) {
    Game game;
    MessageStyle style;

    rngInit(91, 1);
    initGame(&game, MODE_CLASSIC);
    game.fastMode = 1;
    game.saveEnabled = 0;
    ASSERT_EQ_INT(0, simulateRound(&game, 10, 0, 0,
                                   NULL, NULL, &style));
    ASSERT_EQ_INT(STATE_BET, game.state);
    ASSERT_EQ_INT(0, game.roundNumber);
}


static int fixed_bet_policy(const Game *game, void *context) {
    (void)game;
    return *(const int *)context;
}

TEST(classic_session_collects_reproducible_run_statistics) {
    Game first;
    Game second;
    SimulationRunStats firstStats;
    SimulationRunStats secondStats;
    int bet = 10;

    rngInit(77123, 1);
    initGame(&first, MODE_CLASSIC);
    first.fastMode = 1;
    first.saveEnabled = 0;
    ASSERT_EQ_INT(1, simulateClassicRun(&first, 25,
                                        fixed_bet_policy, &bet,
                                        stand_policy, NULL, &firstStats));

    rngInit(77123, 1);
    initGame(&second, MODE_CLASSIC);
    second.fastMode = 1;
    second.saveEnabled = 0;
    ASSERT_EQ_INT(1, simulateClassicRun(&second, 25,
                                        fixed_bet_policy, &bet,
                                        stand_policy, NULL, &secondStats));

    ASSERT_EQ_INT(25, firstStats.rounds);
    ASSERT_EQ_INT(firstStats.rounds, firstStats.wins +
                  firstStats.losses + firstStats.pushes);
    ASSERT_EQ_INT(firstStats.startingChips, 100);
    ASSERT_EQ_INT(firstStats.finalChips, secondStats.finalChips);
    ASSERT_EQ_INT(firstStats.peakChips, secondStats.peakChips);
    ASSERT_EQ_INT(firstStats.wins, secondStats.wins);
    ASSERT_EQ_INT(firstStats.losses, secondStats.losses);
    ASSERT_EQ_INT(firstStats.pushes, secondStats.pushes);
}


static int minimum_bet_policy(const Game *game, void *context) {
    (void)context;
    return getMinBet(game, game->run.floor, game->mode);
}

static char shop_test_floor_policy(const Game *game, void *context) {
    int *calls = (int *)context;
    (void)game;
    if ((*calls)++ == 0) return 's';
    return 'q';
}

static int buy_first_affordable_perk(const Game *game,
                                    int currentMinBet,
                                    void *context) {
    int index;
    (void)context;
    for (index = 0; index < game->run.shopPerkCount; index++) {
        PerkId perk = game->run.shopPerks[index];
        if (game->run.marks >= perkCost(perk)) return (int)perk + 1;
    }
    (void)currentMinBet;
    return 0;
}

TEST(roguelike_runner_executes_policy_shop_purchase) {
    Game game;
    SimulationRunStats stats;
    int floorCalls = 0;
    int ownedBefore = 0;
    int index;

    rngInit(98117, 1);
    initGame(&game, MODE_ROGUELIKE);
    game.fastMode = 1;
    game.saveEnabled = 0;
    game.run.marks = 100;
    for (index = 0; index < PERK_COUNT; index++) {
        ownedBefore += game.player.perkOwned[index];
    }

    ASSERT_EQ_INT(1, simulateRoguelikeRun(
        &game, 1, minimum_bet_policy, NULL,
        stand_policy, NULL, shop_test_floor_policy, &floorCalls,
        buy_first_affordable_perk, NULL, &stats));

    {
        int ownedAfter = 0;
        for (index = 0; index < PERK_COUNT; index++) {
            ownedAfter += game.player.perkOwned[index];
        }
        ASSERT_EQ_INT(ownedBefore + 1, ownedAfter);
    }
    ASSERT_EQ_INT(STATE_QUIT, game.state);
    ASSERT_EQ_INT(1, stats.rounds);
}

static char advance_floor_policy(const Game *game, void *context) {
    (void)game;
    (void)context;
    return 'a';
}

TEST(roguelike_runner_advances_floors_and_tracks_progression) {
    Game first;
    Game second;
    SimulationRunStats firstStats;
    SimulationRunStats secondStats;
    rngInit(441122, 1);
    initGame(&first, MODE_ROGUELIKE);
    first.fastMode = 1;
    first.saveEnabled = 0;
    ASSERT_EQ_INT(1, simulateRoguelikeRun(
        &first, 8, minimum_bet_policy, NULL,
        stand_policy, NULL, advance_floor_policy, NULL, NULL, NULL, &firstStats));

    rngInit(441122, 1);
    initGame(&second, MODE_ROGUELIKE);
    second.fastMode = 1;
    second.saveEnabled = 0;
    ASSERT_EQ_INT(1, simulateRoguelikeRun(
        &second, 8, minimum_bet_policy, NULL,
        stand_policy, NULL, advance_floor_policy, NULL, NULL, NULL, &secondStats));

    ASSERT_TRUE(firstStats.rounds > 0);
    ASSERT_TRUE(firstStats.rounds <= 8);
    ASSERT_EQ_INT(firstStats.rounds, secondStats.rounds);
    ASSERT_TRUE(firstStats.floorsReached > 0);
    ASSERT_TRUE(first.run.floor > 1);
    ASSERT_EQ_INT(first.run.floor, second.run.floor);
    ASSERT_EQ_INT(firstStats.finalChips, secondStats.finalChips);
    ASSERT_EQ_INT(firstStats.marksEarned, secondStats.marksEarned);
    ASSERT_EQ_INT(firstStats.floorsReached, secondStats.floorsReached);
}

void run_simulation_tests(void) {
    RUN_TEST(policy_runner_completes_and_repeats_seeded_round);
    RUN_TEST(policy_runner_rejects_unavailable_policy_action);
    RUN_TEST(classic_session_collects_reproducible_run_statistics);
    RUN_TEST(roguelike_runner_advances_floors_and_tracks_progression);
    RUN_TEST(roguelike_runner_executes_policy_shop_purchase);
}
