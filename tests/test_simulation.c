#include "test.h"
#include "game.h"
#include "round_simulation.h"
#include "round_actions.h"
#include "round_economy.h"
#include "round_cards.h"
#include "round_rules.h"
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

static ActionId progressing_policy(const Game *game,
                                   const ActionId *actions,
                                   int actionCount,
                                   void *context) {
    int index;
    int standBlocked =
        (twistActive(game, TWIST_STICKY_HANDS) &&
         calculatePlayerHandValue(game) < 15) ||
        (game->run.floor >= 10 &&
         game->run.dealerId == DEALER_BUTCHER &&
         calculatePlayerHandValue(game) < 15);
    (void)context;
    for (index = 0; index < actionCount; index++) {
        if (standBlocked && actions[index] == ACTION_HIT) return ACTION_HIT;
        if (!standBlocked && actions[index] == ACTION_STAND) return ACTION_STAND;
        if (standBlocked && actions[index] == ACTION_STAND &&
            game->player.hands[game->player.activeHand].count >=
                MAX_HAND_SIZE) return ACTION_STAND;
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

TEST(full_hand_can_stand_through_low_total_restrictions) {
    Game game;
    ActionId actions[ACTION_COUNT + 1];
    int actionCount;
    int index;
    int standAvailable = 0;

    rngInit(92, 1);
    initGame(&game, MODE_ROGUELIKE);
    game.state = STATE_PLAYER_TURN;
    game.run.twistId = TWIST_STICKY_HANDS;
    game.player.handCount = 1;
    game.player.activeHand = 0;
    game.player.hands[0].count = MAX_HAND_SIZE;
    for (index = 0; index < MAX_HAND_SIZE; index++) {
        game.player.hands[0].cards[index].rank = TWO;
        game.player.hands[0].cards[index].cursed = CURSED_NONE;
    }

    actionCount = getPlayerActions(&game, actions);
    for (index = 0; index < actionCount; index++) {
        if (actions[index] == ACTION_STAND) standAvailable = 1;
        ASSERT_TRUE(actions[index] != ACTION_HIT);
    }
    ASSERT_EQ_INT(1, standAvailable);

    applyPlayerAction(&game, ACTION_STAND);
    ASSERT_EQ_INT(STATE_DEALER_TURN, game.state);
}


static ActionId quit_policy(const Game *game,
                            const ActionId *actions,
                            int actionCount,
                            void *context) {
    int index;
    (void)game;
    (void)context;
    for (index = 0; index < actionCount; index++) {
        if (actions[index] == ACTION_QUIT) return ACTION_QUIT;
    }
    return ACTION_COUNT;
}

TEST(policy_runner_reports_quit_without_failing_or_settling_round) {
    Game game;
    SimulationRunStats stats;
    MessageStyle style;

    rngInit(20261120U, 1);
    initGame(&game, MODE_CLASSIC);
    game.fastMode = 1;
    game.saveEnabled = 0;

    ASSERT_EQ_INT(2, simulateRound(&game, 10, 0, 0,
                                   quit_policy, NULL, &style));
    ASSERT_EQ_INT(STATE_QUIT, game.state);
    ASSERT_EQ_INT(1, game.roundNumber);
    ASSERT_EQ_INT(1, game.roundInProgress);

    rngInit(20261120U, 1);
    initGame(&game, MODE_CLASSIC);
    game.fastMode = 1;
    game.saveEnabled = 0;
    ASSERT_EQ_INT(1, simulateClassicRun(
        &game, 10, minimum_bet_policy, NULL,
        quit_policy, NULL, &stats));
    ASSERT_EQ_INT(0, stats.rounds);
    ASSERT_EQ_INT(SIMULATION_STOP_GAME_QUIT, stats.stopReason);
    ASSERT_EQ_INT(STATE_QUIT, game.state);
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
    ASSERT_EQ_INT(SIMULATION_STOP_MAX_ROUNDS, firstStats.stopReason);
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

TEST(balanced_shop_policy_prioritizes_survival_and_funds_it) {
    Game game;
    rngInit(8128, 1);
    initGame(&game, MODE_ROGUELIKE);
    game.state = STATE_SHOP;
    game.run.marks = 48;
    game.run.shopPerkCount = 3;
    game.run.shopPerks[0] = PERK_BANKROLL;
    game.run.shopPerks[1] = PERK_SURVIVAL;
    game.run.shopPerks[2] = PERK_PEEK;

    ASSERT_EQ_INT(-(int)'v',
                  simulationBalancedShopPolicy(&game, 10, NULL));
    game.run.marks = 50;
    ASSERT_EQ_INT((int)PERK_SURVIVAL + 1,
                  simulationBalancedShopPolicy(&game, 10, NULL));

    game.run.marks = 0;
    game.run.shopPerkCount = 1;
    game.run.shopPerks[0] = PERK_DOUBLE_DIP;
    ASSERT_EQ_INT(-(int)'v',
                  simulationBalancedShopPolicy(&game, 10, NULL));
    game.run.marks = 54;
    ASSERT_EQ_INT(-(int)'v',
                  simulationBalancedShopPolicy(&game, 10, NULL));
    game.run.marks = 56;
    ASSERT_EQ_INT((int)PERK_DOUBLE_DIP + 1,
                  simulationBalancedShopPolicy(&game, 10, NULL));
}

TEST(balanced_shop_policy_skips_unfundable_high_priority_perk) {
    Game game;

    rngInit(8129, 1);
    initGame(&game, MODE_ROGUELIKE);
    game.state = STATE_SHOP;
    game.player.chips = 0;
    game.run.marks = perkCost(PERK_PEEK);
    game.run.shopPerkCount = 2;
    game.run.shopPerks[0] = PERK_SURVIVAL;
    game.run.shopPerks[1] = PERK_PEEK;

    ASSERT_EQ_INT((int)PERK_PEEK + 1,
                  simulationBalancedShopPolicy(&game, 10, NULL));
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
        NULL, NULL, buy_first_affordable_perk, NULL, &stats));

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
        stand_policy, NULL, advance_floor_policy, NULL, NULL, NULL, NULL, NULL, &firstStats));

    rngInit(441122, 1);
    initGame(&second, MODE_ROGUELIKE);
    second.fastMode = 1;
    second.saveEnabled = 0;
    ASSERT_EQ_INT(1, simulateRoguelikeRun(
        &second, 8, minimum_bet_policy, NULL,
        stand_policy, NULL, advance_floor_policy, NULL, NULL, NULL, NULL, NULL, &secondStats));

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


static int disable_boss_rule_policy(const Game *game, void *context) {
    int *calls = (int *)context;
    (void)game;
    (*calls)++;
    return 1;
}

TEST(roguelike_runner_uses_boss_preparation_policy) {
    unsigned int seed;
    int exercised = 0;

    for (seed = 1; seed <= 64 && !exercised; seed++) {
        Game game;
        SimulationRunStats stats;
        int prepCalls = 0;

        rngInit(seed, 1);
        initGame(&game, MODE_ROGUELIKE);
        game.fastMode = 1;
        game.saveEnabled = 0;
        game.run.marks = 1000;
        game.player.chips = 100000;

        ASSERT_EQ_INT(1, simulateRoguelikeRun(
            &game, 32, minimum_bet_policy, NULL,
            stand_policy, NULL, advance_floor_policy, NULL,
            disable_boss_rule_policy, &prepCalls, NULL, NULL, &stats));
        if (prepCalls > 0) {
            ASSERT_EQ_INT(1, game.bossRuleDisabled);
            exercised = 1;
        }
    }

    ASSERT_EQ_INT(1, exercised);
}

typedef struct {
    int attempts;
    int purchases;
} BossPrepComparisonContext;

static int pay_for_boss_rule_when_affordable(const Game *game, void *context) {
    BossPrepComparisonContext *comparison =
        (BossPrepComparisonContext *)context;
    comparison->attempts++;
    if (game->run.marks < game->bossRulePaid) return 0;
    comparison->purchases++;
    return 1;
}

TEST(roguelike_batch_compares_boss_preparation_policies_on_same_seeds) {
    static const unsigned int seeds[] = {
        3, 7, 11, 19, 23, 31, 37, 43,
        47, 59, 61, 67, 71, 79, 83, 89
    };
    SimulationBatchStats baseline;
    SimulationBatchStats paid;
    BossPrepComparisonContext comparison = {0, 0};

    ASSERT_EQ_INT(1, simulateRoguelikeBatch(
        seeds, 16, 32, minimum_bet_policy, NULL,
        progressing_policy, NULL, advance_floor_policy, NULL,
        NULL, NULL, simulationBalancedShopPolicy, NULL, &baseline));
    ASSERT_EQ_INT(1, simulateRoguelikeBatch(
        seeds, 16, 32, minimum_bet_policy, NULL,
        progressing_policy, NULL, advance_floor_policy, NULL,
        pay_for_boss_rule_when_affordable, &comparison,
        simulationBalancedShopPolicy, NULL, &paid));

    ASSERT_EQ_INT(16, baseline.runsCompleted);
    ASSERT_EQ_INT(16, paid.runsCompleted);
    ASSERT_TRUE(comparison.attempts > 0);
    ASSERT_TRUE(comparison.purchases > 0);
    ASSERT_TRUE(baseline.totalFloorsReached >= 16);
    ASSERT_TRUE(paid.totalFloorsReached >= 16);
    ASSERT_TRUE(baseline.totalBossesDefeated >= 0);
    ASSERT_TRUE(paid.totalBossesDefeated >= 0);
    ASSERT_TRUE(baseline.totalFinalChips >= 0);
    ASSERT_TRUE(paid.totalFinalChips >= 0);
    ASSERT_EQ_INT(16, baseline.stopReasonCounts[SIMULATION_STOP_MAX_ROUNDS] +
                  baseline.stopReasonCounts[SIMULATION_STOP_INSUFFICIENT_BANKROLL] +
                  baseline.stopReasonCounts[SIMULATION_STOP_POLICY] +
                  baseline.stopReasonCounts[SIMULATION_STOP_GAME_QUIT]);
    ASSERT_EQ_INT(16, paid.stopReasonCounts[SIMULATION_STOP_MAX_ROUNDS] +
                  paid.stopReasonCounts[SIMULATION_STOP_INSUFFICIENT_BANKROLL] +
                  paid.stopReasonCounts[SIMULATION_STOP_POLICY] +
                  paid.stopReasonCounts[SIMULATION_STOP_GAME_QUIT]);
}

TEST(roguelike_batch_aggregates_reproducible_seeded_runs) {
    static const unsigned int seeds[] = {17, 29, 41, 53};
    SimulationBatchStats first;
    SimulationBatchStats second;

    ASSERT_EQ_INT(1, simulateRoguelikeBatch(
        seeds, 4, 8, minimum_bet_policy, NULL,
        stand_policy, NULL, advance_floor_policy, NULL,
        NULL, NULL, NULL, NULL, &first));
    ASSERT_EQ_INT(1, simulateRoguelikeBatch(
        seeds, 4, 8, minimum_bet_policy, NULL,
        stand_policy, NULL, advance_floor_policy, NULL,
        NULL, NULL, NULL, NULL, &second));

    ASSERT_EQ_INT(4, first.runsCompleted);
    ASSERT_EQ_INT(4, second.runsCompleted);
    ASSERT_EQ_INT(4, first.stopReasonCounts[SIMULATION_STOP_MAX_ROUNDS] +
                  first.stopReasonCounts[SIMULATION_STOP_INSUFFICIENT_BANKROLL] +
                  first.stopReasonCounts[SIMULATION_STOP_POLICY] +
                  first.stopReasonCounts[SIMULATION_STOP_GAME_QUIT]);
    ASSERT_EQ_INT(first.stopReasonCounts[SIMULATION_STOP_MAX_ROUNDS],
                  second.stopReasonCounts[SIMULATION_STOP_MAX_ROUNDS]);
    ASSERT_EQ_INT(first.stopReasonCounts[SIMULATION_STOP_INSUFFICIENT_BANKROLL],
                  second.stopReasonCounts[SIMULATION_STOP_INSUFFICIENT_BANKROLL]);
    ASSERT_EQ_INT(first.stopReasonCounts[SIMULATION_STOP_POLICY],
                  second.stopReasonCounts[SIMULATION_STOP_POLICY]);
    ASSERT_EQ_INT(first.stopReasonCounts[SIMULATION_STOP_GAME_QUIT],
                  second.stopReasonCounts[SIMULATION_STOP_GAME_QUIT]);
    ASSERT_EQ_INT(400, first.totalStartingChips);
    ASSERT_TRUE(first.totalRounds > 0);
    ASSERT_EQ_INT(first.totalRounds,
                  first.totalWins + first.totalLosses + first.totalPushes);
    ASSERT_EQ_INT(first.totalRounds, second.totalRounds);
    ASSERT_EQ_INT(first.totalWins, second.totalWins);
    ASSERT_EQ_INT(first.totalLosses, second.totalLosses);
    ASSERT_EQ_INT(first.totalPushes, second.totalPushes);
    ASSERT_EQ_INT(first.totalFinalChips, second.totalFinalChips);
    ASSERT_EQ_INT(first.totalPeakChips, second.totalPeakChips);
    ASSERT_EQ_INT(first.minFinalChips, second.minFinalChips);
    ASSERT_EQ_INT(first.maxFinalChips, second.maxFinalChips);
    ASSERT_EQ_INT(first.profitableRuns, second.profitableRuns);
    ASSERT_EQ_INT(first.bankruptRuns, second.bankruptRuns);
    ASSERT_TRUE(first.minFinalChips <= first.maxFinalChips);
    ASSERT_TRUE(first.profitableRuns >= 0 && first.profitableRuns <= 4);
    ASSERT_TRUE(first.bankruptRuns >= 0 && first.bankruptRuns <= 4);
    ASSERT_EQ_INT(first.totalFloorsReached, second.totalFloorsReached);
    ASSERT_EQ_INT(first.totalBossesDefeated, second.totalBossesDefeated);
    ASSERT_EQ_INT(first.totalMarksEarned, second.totalMarksEarned);
    ASSERT_EQ_INT(first.shallowestFloorReached,
                  second.shallowestFloorReached);
    ASSERT_EQ_INT(first.deepestFloorReached, second.deepestFloorReached);
    ASSERT_TRUE(first.deepestFloorReached >= first.shallowestFloorReached);
}

TEST(roguelike_runner_stops_when_bet_cap_is_below_minimum) {
    Game game;
    SimulationRunStats stats;

    rngInit(88109U, 1);
    initGame(&game, MODE_ROGUELIKE);
    game.fastMode = 1;
    game.saveEnabled = 0;
    game.run.floor = 8;
    game.player.chips = 60;

    ASSERT_TRUE(getMinBet(&game, game.run.floor, game.mode) == 34);
    ASSERT_TRUE(getMaxBet(&game) == 30);
    ASSERT_EQ_INT(1, simulateRoguelikeRun(
        &game, 100, minimum_bet_policy, NULL,
        progressing_policy, NULL, advance_floor_policy, NULL,
        NULL, NULL, NULL, NULL, &stats));
    ASSERT_EQ_INT(0, stats.rounds);
    ASSERT_EQ_INT(SIMULATION_STOP_INSUFFICIENT_BANKROLL, stats.stopReason);
    ASSERT_EQ_INT(60, stats.finalChips);
}

TEST(roguelike_batch_compares_shop_policies_on_the_same_seed_set) {
    static const unsigned int seeds[] = {
        3,  7,  11, 19, 23, 31, 37, 43,
        47, 59, 61, 67, 71, 79, 83, 89
    };
    SimulationBatchStats baseline;
    SimulationBatchStats balanced;

    ASSERT_EQ_INT(1, simulateRoguelikeBatch(
        seeds, 16, 32, minimum_bet_policy, NULL,
        progressing_policy, NULL, advance_floor_policy, NULL,
        NULL, NULL, NULL, NULL, &baseline));
    ASSERT_EQ_INT(1, simulateRoguelikeBatch(
        seeds, 16, 32, minimum_bet_policy, NULL,
        progressing_policy, NULL, advance_floor_policy, NULL,
        NULL, NULL, simulationBalancedShopPolicy, NULL, &balanced));

    ASSERT_EQ_INT(16, baseline.runsCompleted);
    ASSERT_EQ_INT(16, balanced.runsCompleted);
    ASSERT_EQ_INT(16, baseline.stopReasonCounts[SIMULATION_STOP_MAX_ROUNDS] +
                  baseline.stopReasonCounts[SIMULATION_STOP_INSUFFICIENT_BANKROLL] +
                  baseline.stopReasonCounts[SIMULATION_STOP_POLICY] +
                  baseline.stopReasonCounts[SIMULATION_STOP_GAME_QUIT]);
    ASSERT_EQ_INT(16, balanced.stopReasonCounts[SIMULATION_STOP_MAX_ROUNDS] +
                  balanced.stopReasonCounts[SIMULATION_STOP_INSUFFICIENT_BANKROLL] +
                  balanced.stopReasonCounts[SIMULATION_STOP_POLICY] +
                  balanced.stopReasonCounts[SIMULATION_STOP_GAME_QUIT]);
    ASSERT_TRUE(baseline.totalFloorsReached >= baseline.runsCompleted);
    ASSERT_TRUE(balanced.totalFloorsReached >= balanced.runsCompleted);
    ASSERT_TRUE(baseline.minFinalChips <= baseline.maxFinalChips);
    ASSERT_TRUE(balanced.minFinalChips <= balanced.maxFinalChips);
    ASSERT_TRUE(baseline.profitableRuns >= 0 &&
                baseline.profitableRuns <= baseline.runsCompleted);
    ASSERT_TRUE(balanced.profitableRuns >= 0 &&
                balanced.profitableRuns <= balanced.runsCompleted);
    ASSERT_TRUE(baseline.bankruptRuns >= 0 &&
                baseline.bankruptRuns <= baseline.runsCompleted);
    ASSERT_TRUE(balanced.bankruptRuns >= 0 &&
                balanced.bankruptRuns <= balanced.runsCompleted);
    ASSERT_TRUE(baseline.profitableRuns + baseline.bankruptRuns <=
                baseline.runsCompleted);
    ASSERT_TRUE(balanced.profitableRuns + balanced.bankruptRuns <=
                balanced.runsCompleted);
    ASSERT_TRUE(baseline.deepestFloorReached >=
                baseline.shallowestFloorReached);
    ASSERT_TRUE(balanced.deepestFloorReached >=
                balanced.shallowestFloorReached);
}

void run_simulation_tests(void) {
    RUN_TEST(policy_runner_completes_and_repeats_seeded_round);
    RUN_TEST(full_hand_can_stand_through_low_total_restrictions);
    RUN_TEST(policy_runner_rejects_unavailable_policy_action);
    RUN_TEST(policy_runner_reports_quit_without_failing_or_settling_round);
    RUN_TEST(classic_session_collects_reproducible_run_statistics);
    RUN_TEST(roguelike_runner_advances_floors_and_tracks_progression);
    RUN_TEST(roguelike_runner_executes_policy_shop_purchase);
    RUN_TEST(roguelike_runner_uses_boss_preparation_policy);
    RUN_TEST(balanced_shop_policy_prioritizes_survival_and_funds_it);
    RUN_TEST(balanced_shop_policy_skips_unfundable_high_priority_perk);
    RUN_TEST(roguelike_batch_compares_boss_preparation_policies_on_same_seeds);
    RUN_TEST(roguelike_batch_aggregates_reproducible_seeded_runs);
    RUN_TEST(roguelike_runner_stops_when_bet_cap_is_below_minimum);
    RUN_TEST(roguelike_batch_compares_shop_policies_on_the_same_seed_set);
}
