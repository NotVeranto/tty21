#ifndef ROUND_H
#define ROUND_H

#include "game.h"

void startRound(Game *game);
void playerTurn(Game *game);
void dealerTurn(Game *game);
void resolveRound(Game *game);
int calculatePlayerHandValue(const Game *game);
int calculatePlayerHandValueForIndex(const Game *game,
                                     int handIndex);
int calculateDealerHandValue(const Hand *hand, DealerId id);

#endif
