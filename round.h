#ifndef ROUND_H
#define ROUND_H

#include "game.h"

void startRound(Game *game);
void drawCard(Game *game, Hand *hand, int allowCurse);
void resetPlayerHandToEleven(Game *game);


#endif
