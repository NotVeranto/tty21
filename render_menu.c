#include "render_menu.h"
#include "render_control.h"
#include "render_terminal.h"
#include "render_style.h"
#include "render_layout.h"
#include "terminal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Prints a fixed menu block with disabled entries dimmed.
void renderMenu(const char *title,
                const MenuItem *items,
                int itemCount,
                int disabledMask,
                int selectedIndex,
                int startRow) {
    int terminalWidth;
    int terminalHeight;
    int titlePadding;
    int itemIndex;
    int isDisabled;
    int rowWidth;
    int rowPadding;

    renderInitColors();
    terminalGetSize(&terminalWidth, &terminalHeight);

    if (isatty(STDOUT_FILENO) &&
        !renderTerminalSizeOk(&terminalWidth,
                              &terminalHeight)) {
        renderTooSmallScreen(terminalWidth, terminalHeight);
        return;
    }

    if (isatty(STDOUT_FILENO) &&
        terminalWidth >= MIN_TERMINAL_WIDTH &&
        terminalHeight >= MIN_TERMINAL_HEIGHT) {
        renderHideCursor();
        titlePadding = getCenteredColumn((int)strlen(title)) - 1;

        printf(SCR_CURSOR, startRow - 1, titlePadding + 1);
        printf(SCR_ERASE_EOL);
        printf("%s%s%s", scrBold(), title, scrReset());

        for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
            isDisabled = (disabledMask & (1 << itemIndex)) != 0;
            rowWidth = 7 + (int)strlen(items[itemIndex].label);
            rowPadding = getCenteredColumn(rowWidth);
            renderMenuItemRow(startRow + itemIndex,
                              rowPadding,
                              items[itemIndex].hotkey,
                              items[itemIndex].label,
                              itemIndex == selectedIndex,
                              isDisabled,
                              0);
        }

        fflush(stdout);
    } else {
        titlePadding = (terminalWidth - (int)strlen(title)) / 2;

        if (titlePadding < 0) {
            titlePadding = 0;
        }

        printf("%*s%s\n", titlePadding, "", title);

        for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
            printf("  [%c]  %s\n",
                   items[itemIndex].hotkey,
                   items[itemIndex].label);
        }

        fflush(stdout);
    }
}

