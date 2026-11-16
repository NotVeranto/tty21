#ifndef RENDER_STATUS_H
#define RENDER_STATUS_H

#include "game.h"

void renderStatusLine(const Game *game, int terminalWidth, int padding);
void animateChips(Game *game, int startChips, int endChips);

#endif
