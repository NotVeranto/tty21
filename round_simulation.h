#ifndef ROUND_SIMULATION_H
#define ROUND_SIMULATION_H

#include "game.h"
#include "input_map.h"

typedef ActionId (*RoundPolicy)(const Game *game,
                                const ActionId *actions,
                                int actionCount,
                                void *context);
typedef int (*SimulationBetPolicy)(const Game *game, void *context);
typedef char (*SimulationFloorPolicy)(const Game *game, void *context);
typedef int (*SimulationBossPrepPolicy)(const Game *game, void *context);
/* Return perkIndex + 1, negative service character, or 0 to close the shop. */
typedef int (*SimulationShopPolicy)(const Game *game,
                                    int currentMinBet,
                                    void *context);

/* Default mark-conscious policy: prioritizes durable run upgrades and only
 * converts surplus chips when that can fund an offered perk. */
int simulationBalancedShopPolicy(const Game *game,
                                 int currentMinBet,
                                 void *context);

typedef enum {
    SIMULATION_STOP_MAX_ROUNDS = 0,
    SIMULATION_STOP_INSUFFICIENT_BANKROLL,
    SIMULATION_STOP_POLICY,
    SIMULATION_STOP_GAME_QUIT,
    SIMULATION_STOP_REASON_COUNT
} SimulationStopReason;

typedef struct {
    int rounds;
    SimulationStopReason stopReason;
    int wins;
    int losses;
    int pushes;
    int startingChips;
    int finalChips;
    int peakChips;
    int floorsReached;
    int bossesDefeated;
    int marksEarned;
    int failureReason;
    int failureState;
    int failureFloor;
    int failureDeckTop;
    int failureHandValue;
    int failureHandCount;
    int failurePlayerHandCount;
    int failureActiveHand;
    int failureTwist;
    int failureDealer;
} SimulationRunStats;

typedef struct {
    int runsCompleted;
    int totalRounds;
    int totalWins;
    int totalLosses;
    int totalPushes;
    int totalStartingChips;
    int totalFinalChips;
    int totalPeakChips;
    int minFinalChips;
    int maxFinalChips;
    int profitableRuns;
    int bankruptRuns;
    int totalFloorsReached;
    int totalBossesDefeated;
    int totalMarksEarned;
    int shallowestFloorReached;
    int deepestFloorReached;
    int stopReasonCounts[SIMULATION_STOP_REASON_COUNT];
    int failedSeedIndex;
    unsigned int failedSeed;
    int failureReason;
    int failureState;
    int failureFloor;
    int failureDeckTop;
    int failureHandValue;
    int failureHandCount;
    int failurePlayerHandCount;
    int failureActiveHand;
    int failureTwist;
    int failureDealer;
} SimulationBatchStats;

/* Returns 0 on failure, 1 on settlement, or 2 when the policy quits. */
int simulateRound(Game *game,
                  int betAmount,
                  int insuranceDecision,
                  int evenMoneyDecision,
                  RoundPolicy policy,
                  void *policyContext,
                  MessageStyle *resultStyle);

int simulateClassicRun(Game *game,
                       int maxRounds,
                       SimulationBetPolicy betPolicy,
                       void *betContext,
                       RoundPolicy roundPolicy,
                       void *roundContext,
                       SimulationRunStats *stats);

/* Roguelike floor policy returns 'c' to continue, 'a' to advance, or 'q'
 * to stop. The optional shop policy returns encoded perk/service decisions. */
int simulateRoguelikeRun(Game *game,
                         int maxRounds,
                         SimulationBetPolicy betPolicy,
                         void *betContext,
                         RoundPolicy roundPolicy,
                         void *roundContext,
                         SimulationFloorPolicy floorPolicy,
                         void *floorContext,
                         SimulationBossPrepPolicy bossPrepPolicy,
                         void *bossPrepContext,
                         SimulationShopPolicy shopPolicy,
                         void *shopContext,
                         SimulationRunStats *stats);

/* Runs one fresh roguelike game per seed and aggregates the outcomes.
 * The same seed array can be passed to multiple policies for paired
 * strategy comparisons. Final-bankroll range and profit/bankruptcy counts
 * expose outcome spread beyond the batch mean. */
int simulateRoguelikeBatch(const unsigned int *seeds,
                           int seedCount,
                           int maxRoundsPerRun,
                           SimulationBetPolicy betPolicy,
                           void *betContext,
                           RoundPolicy roundPolicy,
                           void *roundContext,
                           SimulationFloorPolicy floorPolicy,
                           void *floorContext,
                           SimulationBossPrepPolicy bossPrepPolicy,
                           void *bossPrepContext,
                           SimulationShopPolicy shopPolicy,
                           void *shopContext,
                           SimulationBatchStats *stats);

#endif
