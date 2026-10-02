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
#define ROW_LOG_0 33
#define ROW_LOG_1 34
#define ROW_LOG_2 35
#define ROW_LOG_3 36
#define ROW_LOG_4 37
#define ROW_LOG_5 38
#define ROW_EVENT 33
#define ROW_CHIPS 34
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
#include "round.h"
#include "input_map.h"

typedef struct {
    char hotkey;          /* single character shortcut */
    const char *label;    /* description shown next to hotkey */
} MenuItem;

void renderTable(const Game *game, int hideDealerCard);
void renderGameStateFallback(const Game *game, int hideDealerCard);
void renderHideCursor(void);
void renderShowCursor(void);
void renderShop(const Game *game,
                const MenuItem *items,
                int selectedIndex);
void renderActionBar(const ActionId *actions,
                     int actionCount,
                     int selectedIndex,
                     int row);
void renderMessage(const char *msg, int row, MessageStyle style);
void renderMenu(const char *title,
                const MenuItem *items,
                int itemCount,
                int disabledMask,
                int selectedIndex,
                int startRow);

#endif
