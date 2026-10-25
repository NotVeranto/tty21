#ifndef ROUND_INPUT_H
#define ROUND_INPUT_H

#include "game.h"
#include "input_map.h"

KeyEvent makeKeyEvent(char inputCharacter);
int readBet(Game *game);
int readYesNoPrompt(const char *prompt);

#endif
