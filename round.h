#ifndef ROUND_H
#define ROUND_H

#include "game.h"

void startRound(Game *game);
void drawCard(Game *game, Hand *hand, int allowCurse);
void resetPlayerHandToEleven(Game *game);

typedef struct {
    int fee;           // advanceFee
    int nextMinBet;    // getMinBet(floor + 1)
    int postChips;     // chips after fee and due loan are paid
    int affordable;    // 1 if chips cover fee, min bet, and due loan
    int loanDue;       // loan repayment on advance, 0 if none due
} AdvanceForecast;

#endif
