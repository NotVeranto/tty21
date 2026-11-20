#include "game.h"
#include "round_cards.h"
#include "round_economy.h"
#include "round_rules.h"
#include "round_simulation.h"
#include "rng.h"
#include "twists.h"

#include <stdio.h>
#include <string.h>

#define REPORT_RUNS 256
#define REPORT_ROUNDS_PER_RUN 100

static int minimum_bet_policy(const Game *game, void *context) {
    (void)context;
    return getMinBet(game, game->run.floor, game->mode);
}

static ActionId progressing_policy(const Game *game,
                                   const ActionId *actions,
                                   int actionCount,
                                   void *context) {
    int index;
    int handValue;

    if (game->player.handCount > 0 &&
        game->player.activeHand >= 0 &&
        game->player.activeHand < game->player.handCount &&
        game->player.hands[game->player.activeHand].count == 0) {
        for (index = 0; index < actionCount; index++) {
            if (actions[index] == ACTION_QUIT) return ACTION_QUIT;
        }
    }

    handValue = calculatePlayerHandValue(game);
    int standBlocked =
        (twistActive(game, TWIST_STICKY_HANDS) && handValue < 15) ||
        (game->run.floor >= 10 &&
         game->run.dealerId == DEALER_BUTCHER && handValue < 15);
    (void)context;

    /*
     * No-Reshuffle can exhaust the drawable deck while a low hand is still
     * forbidden from standing. Hitting then cannot change the hand, so end
     * this strategy sample cleanly instead of exhausting the action limit.
     */
    if (standBlocked &&
        game->deck.top >= DECK_SIZE - DECK_BURN_SIZE) {
        for (index = 0; index < actionCount; index++) {
            if (actions[index] == ACTION_QUIT) return ACTION_QUIT;
        }
    }

    for (index = 0; index < actionCount; index++) {
        if (standBlocked && actions[index] == ACTION_HIT) return ACTION_HIT;
        if (!standBlocked && actions[index] == ACTION_STAND) return ACTION_STAND;
        if (standBlocked && actions[index] == ACTION_STAND &&
            calculatePlayerHandValue(game) < 15 &&
            game->player.hands[game->player.activeHand].count >=
                MAX_HAND_SIZE) return ACTION_STAND;
    }
    return ACTION_COUNT;
}

typedef struct {
    unsigned int lastSeed;
    int lastFloor;
    int shopsVisited;
    long long marksAtShop[16];
    int visitsByFloor[16];
} ReportFloorContext;

typedef struct {
    int shopActions;
    int perkPurchases;
} ReportShopContext;

static char visit_shop_then_advance_policy(const Game *game, void *context) {
    ReportFloorContext *report = (ReportFloorContext *)context;

    if (report == NULL) return 'a';
    if (report->lastSeed != game->rngSeed) {
        report->lastSeed = game->rngSeed;
        report->lastFloor = 0;
    }
    /*
     * Give each floor a few rounds to generate its normal rewards before
     * visiting the shop. Entering the shop after the first round of every
     * floor produced a mostly empty comparison: the policy had no marks to
     * spend yet, and advancing immediately starved the run of progression.
     */
    if (game->run.roundsOnFloor < 3) return 'c';
    if (report->lastFloor != game->run.floor) {
        report->lastFloor = game->run.floor;
        report->shopsVisited++;
        if (game->run.floor >= 1 && game->run.floor <= 15) {
            report->marksAtShop[game->run.floor] += game->run.marks;
            report->visitsByFloor[game->run.floor]++;
        }
        return 's';
    }

    /*
     * An unaffordable floor advance is a normal gameplay condition, not a
     * simulation failure. Stay on this floor and keep playing until the
     * bankroll can cover the forecast or the run naturally stops.
     */
    {
        AdvanceForecast forecast;
        computeAdvanceForecast(game, &forecast);
        if (!forecast.affordable) return 'c';
    }
    return 'a';
}

static int counted_balanced_shop_policy(const Game *game,
                                        int currentMinBet,
                                        void *context) {
    ReportShopContext *report = (ReportShopContext *)context;
    int action = simulationBalancedShopPolicy(game, currentMinBet, NULL);

    if (report != NULL && action != 0) {
        report->shopActions++;
        if (action > 0) report->perkPurchases++;
    }
    return action;
}

static void print_floor_marks(const char *label,
                              const ReportFloorContext *report) {
    int floor;

    printf("%s\n", label);
    for (floor = 1; floor <= 15; floor++) {
        if (report->visitsByFloor[floor] > 0) {
            printf("  floor %2d: runs %3d | avg marks %.1f\n",
                   floor, report->visitsByFloor[floor],
                   (double)report->marksAtShop[floor] /
                       report->visitsByFloor[floor]);
        }
    }
}

static void print_batch(const char *label, const SimulationBatchStats *stats) {
    double runs = (double)stats->runsCompleted;
    double outcomes = (double)(stats->totalWins + stats->totalLosses +
                                stats->totalPushes);

    printf("%s\n", label);
    printf("  runs: %d | rounds: %d (%.2f/run)\n",
           stats->runsCompleted, stats->totalRounds,
           stats->totalRounds / runs);
    printf("  outcomes: wins %d (%.1f%%) | losses %d (%.1f%%) | "
           "pushes %d (%.1f%%)\n",
           stats->totalWins, outcomes > 0.0 ? 100.0 * stats->totalWins / outcomes : 0.0,
           stats->totalLosses, outcomes > 0.0 ? 100.0 * stats->totalLosses / outcomes : 0.0,
           stats->totalPushes, outcomes > 0.0 ? 100.0 * stats->totalPushes / outcomes : 0.0);
    printf("  final chips: avg %.2f | min %d | max %d\n",
           stats->totalFinalChips / runs,
           stats->minFinalChips, stats->maxFinalChips);
    printf("  profitable: %d/%d (%.1f%%) | bankrupt: %d/%d (%.1f%%)\n",
           stats->profitableRuns, stats->runsCompleted,
           100.0 * stats->profitableRuns / runs,
           stats->bankruptRuns, stats->runsCompleted,
           100.0 * stats->bankruptRuns / runs);
    printf("  floors: avg %.2f | shallowest %d | deepest %d\n",
           stats->totalFloorsReached / runs,
           stats->shallowestFloorReached,
           stats->deepestFloorReached);
    printf("  bosses defeated: %.2f/run | marks earned: %.2f/run\n",
           stats->totalBossesDefeated / runs,
           stats->totalMarksEarned / runs);
    printf("  stop reasons: round-limit %d | bankroll %d | policy %d | quit %d\n",
           stats->stopReasonCounts[SIMULATION_STOP_MAX_ROUNDS],
           stats->stopReasonCounts[SIMULATION_STOP_INSUFFICIENT_BANKROLL],
           stats->stopReasonCounts[SIMULATION_STOP_POLICY],
           stats->stopReasonCounts[SIMULATION_STOP_GAME_QUIT]);
}


typedef struct { PerkId target; int offers; int affordable; int purchases; long long marksAtOffer; long long marksShort; } ReportSinglePerkContext;

static int single_perk_shop_policy(const Game *game, int currentMinBet,
                                   void *context) {
    ReportSinglePerkContext *report = (ReportSinglePerkContext *)context;
    int index;
    (void)currentMinBet;
    if (game == NULL || report == NULL || game->state != STATE_SHOP ||
        game->player.perkOwned[report->target] ||
        isPerkLockedThisFloor(game, report->target)) return 0;
    for (index = 0; index < game->run.shopPerkCount; index++) {
        if (game->run.shopPerks[index] == report->target) {
            report->offers++;
            report->marksAtOffer += game->run.marks;
            if (game->run.marks >= perkCost(report->target)) {
                report->affordable++;
                report->purchases++;
                return (int)report->target + 1;
            }
            report->marksShort += perkCost(report->target) - game->run.marks;
            return 0;
        }
    }
    return 0;
}

static void print_perk_comparison(const char *name,
                                  const SimulationBatchStats *baseline,
                                  const SimulationBatchStats *perk,
                                  int offers, int affordable, int purchases,
                                  long long marksAtOffer, long long marksShort) {
    double runs = (double)perk->runsCompleted;
    double baseRuns = (double)baseline->runsCompleted;
    printf("  %-16s offered %3d | affordable %3d | bought %3d | "
           "marks %.1f | shortfall %.1f | chips %+.2f | floors %+.2f | "
           "bosses %+.2f | bankrupt %+.1f pp\n",
           name, offers, affordable, purchases,
           offers > 0 ? (double)marksAtOffer / offers : 0.0,
           offers > affordable ? (double)marksShort / (offers - affordable) : 0.0,
           perk->totalFinalChips / runs - baseline->totalFinalChips / baseRuns,
           perk->totalFloorsReached / runs - baseline->totalFloorsReached / baseRuns,
           perk->totalBossesDefeated / runs - baseline->totalBossesDefeated / baseRuns,
           100.0 * ((double)perk->bankruptRuns / runs -
                    (double)baseline->bankruptRuns / baseRuns));
}


typedef struct {
    int startingMarks;
    long long finalChips;
    long long floors;
    long long bosses;
    long long bankrupt;
    long long marksAtShop[16];
    int visitsByFloor[16];
} ReportGrantStats;

static int run_starting_mark_grant_batch(
    const unsigned int *seeds, int seedCount, int startingMarks,
    ReportGrantStats *grantStats) {
    int index;
    memset(grantStats, 0, sizeof(*grantStats));
    grantStats->startingMarks = startingMarks;
    for (index = 0; index < seedCount; index++) {
        Game game;
        SimulationRunStats runStats;
        ReportFloorContext floorContext = {0};
        rngInit(seeds[index], 1);
        initGame(&game, MODE_ROGUELIKE);
        game.fastMode = 1;
        game.saveEnabled = 0;
        game.run.marks += startingMarks;
        game.totalMarksEarned += startingMarks;
        if (!simulateRoguelikeRun(
                &game, REPORT_ROUNDS_PER_RUN,
                minimum_bet_policy, NULL, progressing_policy, NULL,
                visit_shop_then_advance_policy, &floorContext,
                NULL, NULL, NULL, NULL, &runStats)) {
            fprintf(stderr, "error: starting-mark experiment failed for seed %u (grant %d)\n",
                    seeds[index], startingMarks);
            return 0;
        }
        grantStats->finalChips += runStats.finalChips;
        grantStats->floors += runStats.floorsReached;
        grantStats->bosses += runStats.bossesDefeated;
        if (runStats.finalChips <= 0) grantStats->bankrupt++;
        {
            int floor;
            for (floor = 1; floor <= 15; floor++) {
                grantStats->marksAtShop[floor] += floorContext.marksAtShop[floor];
                grantStats->visitsByFloor[floor] += floorContext.visitsByFloor[floor];
            }
        }
    }
    return 1;
}

static void print_starting_mark_grant(const ReportGrantStats *stats,
                                      int runs) {
    double count = (double)runs;
    printf("  +%2d starting marks | chips %.2f | floors %.2f | bosses %.2f | bankrupt %.1f%% | shop marks:",
           stats->startingMarks, stats->finalChips / count,
           stats->floors / count, stats->bosses / count,
           100.0 * stats->bankrupt / count);
    {
        int floor;
        for (floor = 1; floor <= 5; floor++) {
            if (stats->visitsByFloor[floor] > 0) {
                printf(" F%d %.1f", floor,
                       (double)stats->marksAtShop[floor] /
                           stats->visitsByFloor[floor]);
            } else {
                printf(" F%d n/a", floor);
            }
        }
    }
    putchar('\n');
}

int main(void) {
    unsigned int seeds[REPORT_RUNS];
    SimulationBatchStats baseline;
    SimulationBatchStats balanced;
    ReportFloorContext baselineFloor = {0};
    ReportFloorContext balancedFloor = {0};
    ReportShopContext balancedShop = {0, 0};
    int index;
    ReportGrantStats grant15, grant30, grant45;

    for (index = 0; index < REPORT_RUNS; index++) {
        seeds[index] = 1000U + (unsigned int)index * 7919U;
    }

    /*
     * Probe each seed independently before aggregating. This keeps a failed
     * run actionable: the batch API intentionally reports only success/fail.
     */
    for (index = 0; index < REPORT_RUNS; index++) {
        SimulationBatchStats probe;
        if (!simulateRoguelikeBatch(
                &seeds[index], 1, REPORT_ROUNDS_PER_RUN,
                minimum_bet_policy, NULL, progressing_policy, NULL,
                visit_shop_then_advance_policy, &baselineFloor,
                NULL, NULL, NULL, NULL, &probe)) {
            fprintf(stderr, "error: baseline failed for seed %u (index %d, reason %d, state %d, floor %d, deck %d, hand %d/%d (active %d, hands %d), twist %d, dealer %d)\n",
                    seeds[index], index, probe.failureReason, probe.failureState, probe.failureFloor, probe.failureDeckTop, probe.failureHandValue, probe.failureHandCount, probe.failureActiveHand, probe.failurePlayerHandCount, probe.failureTwist, probe.failureDealer);
            return 1;
        }
        if (!simulateRoguelikeBatch(
                &seeds[index], 1, REPORT_ROUNDS_PER_RUN,
                minimum_bet_policy, NULL, progressing_policy, NULL,
                visit_shop_then_advance_policy, &balancedFloor,
                NULL, NULL,
                counted_balanced_shop_policy, &balancedShop, &probe)) {
            fprintf(stderr, "error: balanced shop policy failed for seed %u (index %d)\n",
                    seeds[index], index);
            return 1;
        }
    }

    baselineFloor = (ReportFloorContext){0};
    balancedFloor = (ReportFloorContext){0};
    balancedShop = (ReportShopContext){0, 0};

    if (!simulateRoguelikeBatch(
            seeds, REPORT_RUNS, REPORT_ROUNDS_PER_RUN,
            minimum_bet_policy, NULL, progressing_policy, NULL,
            visit_shop_then_advance_policy, &baselineFloor,
            NULL, NULL, NULL, NULL, &baseline) ||
        !simulateRoguelikeBatch(
            seeds, REPORT_RUNS, REPORT_ROUNDS_PER_RUN,
            minimum_bet_policy, NULL, progressing_policy, NULL,
            visit_shop_then_advance_policy, &balancedFloor,
            NULL, NULL,
            counted_balanced_shop_policy, &balancedShop, &balanced)) {
        fprintf(stderr, "error: simulation batch failed\n");
        return 1;
    }

    if (balancedShop.perkPurchases == 0) {
        fprintf(stderr, "error: balanced policy made no perk purchases\n");
        return 1;
    }

    printf("tty21 seeded shop-policy comparison\n");
    printf("shared seeds: %d | maximum rounds per run: %d\n",
           REPORT_RUNS, REPORT_ROUNDS_PER_RUN);
    printf("shop visits: baseline %d | balanced %d\n\n",
           baselineFloor.shopsVisited, balancedFloor.shopsVisited);
    print_floor_marks("Baseline marks at first shop visit by floor (after at least 3 rounds):",
                      &baselineFloor);
    putchar('\n');
    print_batch("Baseline (no shop purchases)", &baseline);
    putchar('\n');
    print_batch("Balanced shop policy", &balanced);
    printf("  shop actions: %d | perk purchases: %d\n",
           balancedShop.shopActions, balancedShop.perkPurchases);
    puts("\nStarting-mark grant experiment (same seeds, no shop purchases)");
    if (!run_starting_mark_grant_batch(seeds, REPORT_RUNS, 15, &grant15) ||
        !run_starting_mark_grant_batch(seeds, REPORT_RUNS, 30, &grant30) ||
        !run_starting_mark_grant_batch(seeds, REPORT_RUNS, 45, &grant45)) {
        return 1;
    }
    print_starting_mark_grant(&grant15, REPORT_RUNS);
    print_starting_mark_grant(&grant30, REPORT_RUNS);
    print_starting_mark_grant(&grant45, REPORT_RUNS);
    puts("  Starting marks are granted once before floor 1; all gameplay, bet, round, and floor policies remain unchanged.");
    puts("\nSingle-perk purchases (delta versus no-purchase baseline)");
    {
        static const PerkId evaluatedPerks[] = {
            PERK_SECOND_WIND, PERK_PEEK, PERK_SURVIVAL, PERK_ACE_HIGH,
            PERK_SIXTH_SENSE, PERK_DEALERS_FAVOR, PERK_SPLIT_MASTERY,
            PERK_DOUBLE_DIP, PERK_COMPOUNDING, PERK_INSURANCE_PERK,
            PERK_MARKED_CARD, PERK_BANKROLL, PERK_EXTRA_LIFE
        };
        int perkIndex;
        for (perkIndex = 0;
             perkIndex < (int)(sizeof(evaluatedPerks) / sizeof(evaluatedPerks[0]));
             perkIndex++) {
            ReportFloorContext floorContext = {0};
            ReportSinglePerkContext perkContext = {evaluatedPerks[perkIndex], 0, 0, 0, 0LL, 0LL};
            SimulationBatchStats perkStats;
            const char *name = perkName(evaluatedPerks[perkIndex]);
            if (!simulateRoguelikeBatch(
                    seeds, REPORT_RUNS, REPORT_ROUNDS_PER_RUN,
                    minimum_bet_policy, NULL, progressing_policy, NULL,
                    visit_shop_then_advance_policy, &floorContext,
                    NULL, NULL, single_perk_shop_policy, &perkContext,
                    &perkStats)) {
                fprintf(stderr, "error: single-perk simulation failed for %s\n", name);
                return 1;
            }
            print_perk_comparison(name, &baseline, &perkStats,
                                  perkContext.offers, perkContext.affordable,
                                  perkContext.purchases, perkContext.marksAtOffer,
                                  perkContext.marksShort);
        }
    }
    puts("\nNote: same seeds and baseline policies are used for each perk. "
         "Only affordable offers are purchased; this remains a strategy sample, "
         "not a balance verdict.");
    return 0;
}
