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

#define ROW_SHOP_TITLE 10
#define ROW_SHOP_SOULS 11
#define ROW_SHOP_ITEM_0 13
#define ROW_SHOP_ITEM_1 15
#define ROW_SHOP_ITEM_2 17
#define ROW_SHOP_ITEM_3 19
#define ROW_SHOP_ITEM_4 21
#define ROW_SHOP_DESC_0 14
#define ROW_SHOP_DESC_1 16
#define ROW_SHOP_DESC_2 18
#define ROW_SHOP_DESC_3 20
#define ROW_SHOP_DESC_4 22
#define ROW_SHOP_HINT 24

#include "game.h"

typedef enum {
    MSG_DEFAULT,
    MSG_WIN,
    MSG_LOSS,
    MSG_PUSH,
    MSG_INFO
} MessageStyle;

void renderTable(const Game *game, int hideDealerCard);
void renderShop(const Game *game);
void renderMessage(const char *msg, int row, MessageStyle style);
void renderMenu(const char *title,
                const char *const *items,
                int itemCount,
                int disabledMask,
                int startRow);

#endif
