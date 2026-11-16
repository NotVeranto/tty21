#ifndef RENDER_CARDS_H
#define RENDER_CARDS_H

#include "game.h"

void renderDrawDealerHands(const Game *game, int padding, int hideFirstCard);
void renderDrawPlayerHands(const Game *game, int padding);

#endif
