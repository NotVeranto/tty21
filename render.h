#ifndef RENDER_H
#define RENDER_H

#define MIN_TERMINAL_WIDTH  70
#define MIN_TERMINAL_HEIGHT 42

#define SCR_CLEAR "\033[2J"
#define SCR_HOME "\033[H"
#define SCR_CURSOR "\033[%d;%dH"
#define SCR_ERASE_EOL "\033[K"

#define ROW_STATUS 21
#define ROW_HINT 22
#define ROW_RULE 23
#define ROW_TWIST 24
#define ROW_PROMPT 25
#define ROW_ORACLE 26
#define ROW_RESULT 27
#define ROW_LOG_0 28
#define ROW_LOG_1 29
#define ROW_LOG_2 30
#define ROW_LOG_3 31
#define ROW_LOG_4 32
#define ROW_LOG_5 33
#define ROW_MENU_START 35
#define ROW_OVER 40
#define ROW_FINAL 41

#define ROW_SHOP_TITLE 2
#define ROW_SHOP_MARKS 4
#define ROW_SHOP_CHIPS 5
#define ROW_SHOP_HINT 25

#include "game.h"
#include "round.h"
#include "input_map.h"

typedef struct {
    char hotkey;          // single character shortcut
    const char *label;    // description shown next to hotkey
} MenuItem;

int renderTerminalDimensionsOk(int width, int height);
int renderTerminalSizeOk(int *width, int *height);
void renderTooSmallScreen(int currentWidth,
                          int currentHeight);
void renderTable(const Game *game, int hideDealerCard);
void renderStatsScreen(const Game *game);
void renderPause(const Game *game, int ms);
void animateChips(Game *game, int startChips, int endChips);
void renderFloorTransition(const Game *game);
void renderResultFlash(const Game *game,
                       const char *message,
                       MessageStyle style);
void renderBossPrep(const Game *game);
void renderResetScreen(void);
void renderGameStateFallback(const Game *game, int hideDealerCard);
void renderHideCursor(void);
void renderShowCursor(void);
void renderShop(const Game *game,
                const MenuItem *items,
                const char **descriptions,
                const int *groups,
                int itemCount,
                int disabledMask,
                int selectedIndex);
void renderActionBar(const ActionId *actions,
                     int actionCount,
                     int selectedIndex,
                     int row,
                     int showHelp);
void renderHelpOverlay(const Game *game);
void renderMessage(const char *msg, int row, MessageStyle style);
void renderMenuLogRefresh(const Game *game);
void renderMenu(const char *title,
                const MenuItem *items,
                int itemCount,
                int disabledMask,
                int selectedIndex,
                int startRow);

#endif
