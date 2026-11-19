#ifndef ROUND_H
#define ROUND_H

#include "game.h"

int startRoundWithBet(Game *game, int betAmount,
                      int insuranceDecision, int evenMoneyDecision);
void startRound(Game *game);
void drawCard(Game *game, Hand *hand, int allowCurse);
void resetPlayerHandToEleven(Game *game);


#endif
