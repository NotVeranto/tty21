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
#include "render_table.h"
#include "render_menu.h"
#include "render_shop.h"
#include "render_actions.h"
#include "render_stats.h"
#include "render_help.h"
#include "render_transitions.h"

int renderTerminalDimensionsOk(int width, int height);
int renderTerminalSizeOk(int *width, int *height);
void renderTooSmallScreen(int currentWidth,
                          int currentHeight);
void renderGameStateFallback(const Game *game, int hideDealerCard);
void renderHideCursor(void);
void renderShowCursor(void);

#endif
