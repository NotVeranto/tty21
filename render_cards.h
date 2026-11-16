#ifndef RENDER_CARDS_H
#define RENDER_CARDS_H

#include "game.h"

#define CARD_WIDTH 11
#define CARD_GAP 1

void renderDrawDealerHands(const Game *game, int padding, int hideFirstCard);
void renderDrawPlayerHands(const Game *game, int padding);

#endif
