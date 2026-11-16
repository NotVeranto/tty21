#ifndef RENDER_H
#define RENDER_H

#include "render_control.h"
#include "render_rows.h"
#include "terminal.h"
#include "game.h"
#include "round.h"
#include "input_map.h"
#include "render_status.h"
#include "render_messages.h"

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
void renderFloorTransition(const Game *game);
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
void renderMenuLogRefresh(const Game *game);
void renderMenu(const char *title,
                const MenuItem *items,
                int itemCount,
                int disabledMask,
                int selectedIndex,
                int startRow);

#endif
