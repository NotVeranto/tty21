#ifndef ROUND_H
#define ROUND_H

#include "game.h"

int startRoundWithBet(Game *game, int betAmount,
                      int insuranceDecision, int evenMoneyDecision);
void startRound(Game *game);
int remainingDeckCards(const Game *game);
int drawCard(Game *game, Hand *hand, int allowCurse);
void resetPlayerHandToEleven(Game *game);


#endif
