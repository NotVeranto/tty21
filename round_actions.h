#ifndef ROUND_ACTIONS_H
#define ROUND_ACTIONS_H

#include "game.h"
#include "input_map.h"

int getPlayerActions(const Game *game, ActionId *actions);
void applyPlayerAction(Game *game, ActionId action);
void playerTurn(Game *game);

#endif
