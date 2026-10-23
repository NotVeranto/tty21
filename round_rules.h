#ifndef ROUND_RULES_H
#define ROUND_RULES_H

#include "game.h"

int effectiveDealerLossMultiplier(const Game *game);
int effectiveDealerBlackjackNumerator(const Game *game);
int effectiveDealerBlackjackDenominator(const Game *game);
int effectiveDealerMirrorsOnHit(const Game *game);
int effectiveDealerAcesCountAsOne(const Game *game, DealerId id);
int twistActive(const Game *game, TwistId id);
void applyAntiFarmPressure(Game *game);
int fatiguePercent(int roundsOnFloor);

#endif
