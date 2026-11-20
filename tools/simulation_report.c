#include "game.h"
#include "round_cards.h"
#include "round_economy.h"
#include "round_rules.h"
#include "round_simulation.h"
#include "twists.h"

#include <stdio.h>

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
    int handValue = calculatePlayerHandValue(game);
    int standBlocked =
        (twistActive(game, TWIST_STICKY_HANDS) && handValue < 15) ||
        (game->run.floor >= 10 &&
         game->run.dealerId == DEALER_BUTCHER && handValue < 15);
    (void)context;

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

static char advance_floor_policy(const Game *game, void *context) {
    (void)game;
    (void)context;
    return 'a';
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

int main(void) {
    unsigned int seeds[REPORT_RUNS];
    SimulationBatchStats baseline;
    SimulationBatchStats balanced;
    int index;

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
                advance_floor_policy, NULL, NULL, NULL, NULL, NULL,
                &probe)) {
            fprintf(stderr, "error: baseline failed for seed %u (index %d)\\n",
                    seeds[index], index);
            return 1;
        }
        if (!simulateRoguelikeBatch(
                &seeds[index], 1, REPORT_ROUNDS_PER_RUN,
                minimum_bet_policy, NULL, progressing_policy, NULL,
                advance_floor_policy, NULL, NULL, NULL,
                simulationBalancedShopPolicy, NULL, &probe)) {
            fprintf(stderr, "error: balanced shop policy failed for seed %u (index %d)\\n",
                    seeds[index], index);
            return 1;
        }
    }

    if (!simulateRoguelikeBatch(
            seeds, REPORT_RUNS, REPORT_ROUNDS_PER_RUN,
            minimum_bet_policy, NULL, progressing_policy, NULL,
            advance_floor_policy, NULL, NULL, NULL, NULL, NULL,
            &baseline) ||
        !simulateRoguelikeBatch(
            seeds, REPORT_RUNS, REPORT_ROUNDS_PER_RUN,
            minimum_bet_policy, NULL, progressing_policy, NULL,
            advance_floor_policy, NULL, NULL, NULL,
            simulationBalancedShopPolicy, NULL, &balanced)) {
        fprintf(stderr, "error: simulation batch failed\n");
        return 1;
    }

    printf("tty21 seeded shop-policy comparison\n");
    printf("shared seeds: %d | maximum rounds per run: %d\n\n",
           REPORT_RUNS, REPORT_ROUNDS_PER_RUN);
    print_batch("Baseline (no shop purchases)", &baseline);
    putchar('\n');
    print_batch("Balanced shop policy", &balanced);
    puts("\nNote: this is a deterministic strategy sample, not a balance verdict.");
    return 0;
}
