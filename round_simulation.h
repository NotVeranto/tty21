#ifndef ROUND_SIMULATION_H
#define ROUND_SIMULATION_H

#include "game.h"
#include "input_map.h"

typedef ActionId (*RoundPolicy)(const Game *game,
                                const ActionId *actions,
                                int actionCount,
                                void *context);

/* Returns 1 when the round is fully settled, 0 for invalid or incomplete runs. */
int simulateRound(Game *game,
                  int betAmount,
                  int insuranceDecision,
                  int evenMoneyDecision,
                  RoundPolicy policy,
                  void *policyContext,
                  MessageStyle *resultStyle);

#endif
