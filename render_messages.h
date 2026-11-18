#ifndef RENDER_MESSAGES_H
#define RENDER_MESSAGES_H

#include "game.h"

void renderResultFlash(const Game *game,
                       const char *message,
                       MessageStyle style);
void renderMessage(const char *msg,
                   int row,
                   MessageStyle style,
                   int padding);

#endif
