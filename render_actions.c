#include "render_actions.h"
#include "render_canvas.h"
#include "render_control.h"
#include "render_scene.h"
#include "render_terminal.h"
#include "terminal.h"
#include "render_style.h"
#include "render_layout.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Renders the horizontal player action bar as a row of key caps. */
void renderActionBar(const ActionId *actions,
                     int actionCount,
                     int selectedIndex,
                     int row,
                     int showHelp) {
    static const char *ACTION_NAMES[ACTION_COUNT] = {
        [ACTION_HIT] = "Hit",
        [ACTION_STAND] = "Stand",
        [ACTION_DOUBLE] = "Double",
        [ACTION_SPLIT] = "Split",
        [ACTION_PEEK] = "Peek",
        [ACTION_SURRENDER] = "Surrender",
        [ACTION_QUIT] = "Quit"
    };
    _Static_assert(sizeof(ACTION_NAMES) / sizeof(ACTION_NAMES[0]) ==
                       ACTION_COUNT,
                   "action labels must match ActionId");
    int actionIndex;
    int actionId;
    const char *actionName;
    char actionKey;
    char cap[8];
    char label[48];
    int terminalWidth;
    int terminalHeight;
    int visibleWidth;
    int gap;
    int regionX;
    int regionW;
    int x;

    if (isatty(STDOUT_FILENO) &&
        !renderTerminalSizeOk(&terminalWidth,
                              &terminalHeight)) {
        renderTooSmallScreen(terminalWidth, terminalHeight);
        return;
    }

    if (!isatty(STDOUT_FILENO) ||
        terminalWidth < MIN_TERMINAL_WIDTH ||
        terminalHeight < MIN_TERMINAL_HEIGHT ||
        actionCount <= 0 ||
        selectedIndex < 0 ||
        selectedIndex >= actionCount ||
        !canvasBegin()) {
        return;
    }

    renderHudRegion(&regionX, &regionW);
    if (regionW < 20) {
        regionX = 1;
        regionW = terminalWidth;
    }

    /* each entry is " K " + " Name " */
    gap = 2;
    visibleWidth = showHelp ? gap + 8 : 0;
    for (actionIndex = 0; actionIndex < actionCount; actionIndex++) {
        actionId = (int)actions[actionIndex];
        actionName = actionId >= 0 && actionId < ACTION_COUNT
                         ? ACTION_NAMES[actionId] : "Unknown";
        visibleWidth += (actionIndex > 0 ? gap : 0) + 3 + 2 +
                        (int)strlen(actionName);
        if (actionIndex == selectedIndex && !renderAnsiEnabled()) {
            visibleWidth += 2;
        }
    }
    if (visibleWidth > regionW) {
        gap = 1;
        visibleWidth -= actionCount - 1 + (showHelp ? 1 : 0);
    }
    x = regionX + (regionW - visibleWidth) / 2;
    if (x < regionX) {
        x = regionX;
    }

    canvasFill(regionX, row, regionW, 1, ST_PLAIN);
    canvasTouchRow(row);
    for (actionIndex = 0; actionIndex < actionCount; actionIndex++) {
        actionId = (int)actions[actionIndex];
        if (actionId >= 0 && actionId < ACTION_COUNT) {
            actionName = ACTION_NAMES[actionId];
            actionKey = (char)actionToChar(actions[actionIndex]);
        } else {
            actionName = "Unknown";
            actionKey = '?';
        }
        snprintf(cap, sizeof(cap), " %c ", actionKey);
        snprintf(label, sizeof(label), " %s ", actionName);
        if (actionIndex > 0) {
            x += gap;
        }
        if (actionIndex == selectedIndex) {
            if (!renderAnsiEnabled()) {
                x += canvasPutText(x, row, ">", ST_PLAIN);
            }
            x += canvasPutText(x, row, cap, ST_SEL_KEY);
            x += canvasPutText(x, row, label, ST_SEL);
            if (!renderAnsiEnabled()) {
                x += canvasPutText(x, row, "<", ST_PLAIN);
            }
        } else {
            x += canvasPutText(x, row, cap, ST_KEYCAP);
            x += canvasPutText(x, row, label, ST_PLAIN);
        }
    }
    if (showHelp) {
        x += gap;
        x += canvasPutText(x, row, " ? ", ST_KEYCAP);
        canvasPutText(x, row, " help", ST_MUTED);
    }
    canvasFlush();
}
