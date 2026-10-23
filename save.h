#ifndef SAVE_H
#define SAVE_H

#include "game.h"

int saveExists(void);
int saveGame(const Game *game);
int loadGame(Game *game);
void deleteSave(void);

#endif
