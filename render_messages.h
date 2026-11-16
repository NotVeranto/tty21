#ifndef RENDER_MESSAGES_H
#define RENDER_MESSAGES_H

#include "game.h"

void renderMessagesSetPadding(int padding);
void renderResultFlash(const Game *game,
                       const char *message,
                       MessageStyle style);
void renderMessage(const char *msg, int row, MessageStyle style);

#endif
