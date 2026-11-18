#ifndef RENDER_TABLE_H
#define RENDER_TABLE_H

#include "game.h"

void renderTable(const Game *game, int hideDealerCard);
void renderTableMessage(const char *message,
                        int row,
                        MessageStyle style);
void renderPause(const Game *game, int ms);
void renderResetScreen(void);
void renderMenuLogRefresh(const Game *game);

#endif
