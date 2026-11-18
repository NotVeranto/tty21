#include "render_actions.h"
#include "render_control.h"
#include "render_terminal.h"
#include "terminal.h"
#include "render_style.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Renders the horizontal player action bar.
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
    int terminalWidth;
    int terminalHeight;
    int visibleWidth;
    int startColumn;

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
        selectedIndex >= actionCount) {
        return;
    }

    visibleWidth = showHelp ? 11 : 0;
    for (actionIndex = 0; actionIndex < actionCount; actionIndex++) {
        actionId = (int)actions[actionIndex];
        if (actionId >= 0 && actionId < ACTION_COUNT) {
            actionName = ACTION_NAMES[actionId];
        } else {
            actionName = "Unknown";
        }
        visibleWidth += (actionIndex > 0 ? 3 : 0) +
                        6 + (int)strlen(actionName);
    }
    startColumn = (terminalWidth - visibleWidth) / 2 + 1;
    if (startColumn < 1) startColumn = 1;

    printf(SCR_CURSOR, row, startColumn);
    printf(SCR_ERASE_EOL);
    for (actionIndex = 0; actionIndex < actionCount; actionIndex++) {
        actionId = (int)actions[actionIndex];
        if (actionId >= 0 && actionId < ACTION_COUNT) {
            actionName = ACTION_NAMES[actionId];
            actionKey = (char)actionToChar(actions[actionIndex]);
        } else {
            actionName = "Unknown";
            actionKey = '?';
        }

        if (actionIndex > 0) {
            printf("   ");
        }

        if (actionIndex == selectedIndex) {
            printf("%s> %s[%c]%s %s%s%s",
                   scrBold(),
                   scrDim(),
                   actionKey,
                   scrReset(),
                   scrBold(),
                   actionName,
                   scrReset());
        } else {
            printf("  %s[%c]%s %s",
                   scrDim(),
                   actionKey,
                   scrReset(),
                   actionName);
        }
    }

    if (showHelp) {
        printf("   %s[?]%s %shelp%s",
               scrDim(), scrReset(), scrDim(), scrReset());
    }
    fflush(stdout);
}
