#ifndef ROUND_ECONOMY_H
#define ROUND_ECONOMY_H

#include "game.h"

typedef struct {
    int fee;
    int nextMinBet;
    int postChips;
    int affordable;
    int loanDue;
} AdvanceForecast;

void creditChips(Game *game, int amount);
int applyLoss(Game *game, long long lossValue);
void computeAdvanceForecast(const Game *game,
                            AdvanceForecast *out);
void roundSettleInsurance(Game *game);

#endif
