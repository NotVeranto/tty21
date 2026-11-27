#ifndef ROUND_INPUT_H
#define ROUND_INPUT_H

#include "game.h"
#include "input.h"
#include "input_map.h"

KeyEvent makeKeyEvent(int inputCharacter);
int readBet(Game *game);
int readYesNoPrompt(const char *prompt);

#endif
