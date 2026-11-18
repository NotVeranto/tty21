#ifndef ROUND_ECONOMY_H
#define ROUND_ECONOMY_H

#include "run.h"

typedef struct Game Game;

typedef struct {
    int fee;
    int nextMinBet;
    int postChips;
    int affordable;
    int loanDue;
} AdvanceForecast;

int getMinBet(const Game *game, int floor, GameMode mode);
int getMaxBet(const Game *game);
void creditChips(Game *game, int amount);
int applyLoss(Game *game, long long lossValue);
int applyLossForSettlement(Game *game, long long lossValue);
void computeAdvanceForecast(const Game *game,
                            AdvanceForecast *out);
void roundSettleInsurance(Game *game);

#endif
