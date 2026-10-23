#ifndef ROUND_CARDS_H
#define ROUND_CARDS_H

#include "game.h"

int calculateDealerHandValue(const Game *game,
                             const Hand *hand,
                             DealerId id);
int calculatePlayerHandValue(const Game *game);
int calculatePlayerHandValueForIndex(const Game *game,
                                     int handIndex);

#endif
