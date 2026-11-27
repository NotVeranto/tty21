#ifndef RENDER_CARDS_H
#define RENDER_CARDS_H

#include "game.h"

#define CARD_WIDTH 11
#define CARD_GAP 1

int getDynamicTableWidth(const Game *game);

/* Draw one side of the table into the canvas using the current layout.
 * `padding` is kept for source compatibility; card positions come from
 * the layout so that both sides are centred inside the table frame. */
void renderDrawDealerHands(const Game *game, int padding, int hideFirstCard);
void renderDrawPlayerHands(const Game *game, int padding);

#endif
