#ifndef RENDER_H
#define RENDER_H

#define SCR_RESET "\033[0m"
#define SCR_RED "\033[31m"
#define SCR_BOLD "\033[1m"
#define SCR_DIM "\033[2m"
#define SCR_CLEAR "\033[2J"
#define SCR_HOME "\033[H"
#define SCR_CURSOR "\033[%d;%dH"
#define SCR_ERASE_EOL "\033[K"

#include "game.h"

void renderTable(const Game *game, int hideDealerCard);
void renderMessage(const char *msg, int row);

#endif
