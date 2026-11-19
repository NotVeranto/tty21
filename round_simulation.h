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
/* Return perkIndex + 1, negative service character, or 0 to close the shop. */
typedef int (*SimulationShopPolicy)(const Game *game,
                                    int currentMinBet,
                                    void *context);

typedef struct {
    int rounds;
    int wins;
    int losses;
    int pushes;
    int startingChips;
    int finalChips;
    int peakChips;
    int floorsReached;
    int bossesDefeated;
    int marksEarned;
} SimulationRunStats;

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
 * to stop. Shop decisions are deliberately left to a later policy layer. */
int simulateRoguelikeRun(Game *game,
                         int maxRounds,
                         SimulationBetPolicy betPolicy,
                         void *betContext,
                         RoundPolicy roundPolicy,
                         void *roundContext,
                         SimulationFloorPolicy floorPolicy,
                         void *floorContext,
                         SimulationShopPolicy shopPolicy,
                         void *shopContext,
                         SimulationRunStats *stats);

#endif
