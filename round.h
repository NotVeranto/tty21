#ifndef ROUND_H
#define ROUND_H

#include "game.h"

void startRound(Game *game);
void playerTurn(Game *game);
void dealerTurn(Game *game);
void resolveRound(Game *game);

typedef struct {
    int fee;           /* advanceFee */
    int nextMinBet;    /* getMinBet(floor + 1) */
    int postChips;     /* chips after fee and due loan are paid */
    int affordable;    /* 1 if chips cover fee, min bet, and due loan */
    int loanDue;       /* loan repayment on advance, 0 if none due */
} AdvanceForecast;

#endif
