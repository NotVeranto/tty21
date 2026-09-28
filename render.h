#ifndef RENDER_H
#define RENDER_H

#define SCR_CLEAR "\033[2J"
#define SCR_HOME "\033[H"
#define SCR_CURSOR "\033[%d;%dH"
#define SCR_ERASE_EOL "\033[K"

#define ROW_STATUS 26
#define ROW_HINT 27
#define ROW_RULE 28
#define ROW_TWIST 29
#define ROW_PROMPT 30
#define ROW_ORACLE 31
#define ROW_RESULT 32
#define ROW_CHIPS 33
#define ROW_MENU_START 36
#define ROW_OVER 39
#define ROW_FINAL 40

#include "game.h"

typedef enum {
    MSG_DEFAULT,
    MSG_WIN,
    MSG_LOSS,
    MSG_PUSH,
    MSG_INFO
} MessageStyle;

void renderTable(const Game *game, int hideDealerCard);
void renderMessage(const char *msg, int row, MessageStyle style);
void renderMenu(const char *title,
                const char *const *items,
                int itemCount,
                int disabledMask,
                int startRow);

#endif
