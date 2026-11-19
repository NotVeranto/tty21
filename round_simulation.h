#ifndef ROUND_SIMULATION_H
#define ROUND_SIMULATION_H

#include "game.h"
#include "input_map.h"

typedef ActionId (*RoundPolicy)(const Game *game,
                                const ActionId *actions,
                                int actionCount,
                                void *context);
typedef int (*SimulationBetPolicy)(const Game *game, void *context);

typedef struct {
    int rounds;
    int wins;
    int losses;
    int pushes;
    int startingChips;
    int finalChips;
    int peakChips;
} SimulationRunStats;

/* Runs and settles one round; returns 1 on completion, 0 on invalid/incomplete. */
int simulateRound(Game *game,
                  int betAmount,
                  int insuranceDecision,
                  int evenMoneyDecision,
                  RoundPolicy policy,
                  void *policyContext,
                  MessageStyle *resultStyle);

/* Runs a Classic session until the bet policy stops, bankroll is insufficient,
 * or maxRounds is reached. Returns 1 for a completed session. */
int simulateClassicRun(Game *game,
                       int maxRounds,
                       SimulationBetPolicy betPolicy,
                       void *betContext,
                       RoundPolicy roundPolicy,
                       void *roundContext,
                       SimulationRunStats *stats);

#endif
