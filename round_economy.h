#ifndef ROUND_ECONOMY_H
#define ROUND_ECONOMY_H

#include "round.h"

void creditChips(Game *game, int amount);
int applyLoss(Game *game, long long lossValue);
void computeAdvanceForecast(const Game *game,
                            AdvanceForecast *out);
void roundSettleInsurance(Game *game);

#endif
