#ifndef ROUND_PRESENTATION_H
#define ROUND_PRESENTATION_H

#include "game.h"
#include "input_map.h"

typedef enum {
    ROUND_MESSAGE_PROMPT,
    ROUND_MESSAGE_RESULT
} RoundMessageLocation;

void roundPresentTable(const Game *game, int hideDealerCard);
void roundPresentMessage(const char *message,
                         RoundMessageLocation location,
                         MessageStyle style);
void roundPause(const Game *game, int ms);
void roundPresentActionBar(const ActionId *actions,
                           int actionCount,
                           int selectedIndex,
                           int showHelp);
void roundPresentHelp(const Game *game);

#endif
