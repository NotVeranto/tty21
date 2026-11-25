#include "render_shop.h"
#include "render_control.h"
#include "render_rows.h"
#include "render_terminal.h"
#include "render_layout.h"
#include "render_cards.h"
#include "render_style.h"
#include "render_log.h"
#include "terminal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SHOP_ITEM_START_ROW 7
#define SHOP_DESCRIPTION_ROW 24
#define SHOP_HINT_ROW 25

static void moveToTableRow(int row, int padding) {
    printf(SCR_CURSOR, row, padding + 1);
}



void renderShop(const Game *game,
                const MenuItem *items,
                const char **descriptions,
                const int *groups,
                int itemCount,
                int disabledMask,
                int selectedIndex) {
    static const char *GROUP_TITLES[] = {
        "PERKS", "SERVICES", "TWIST TOOLS"
    };
    int terminalWidth;
    int terminalHeight;
    int tableWidth;
    int shopPadding;
    int itemIndex;
    int itemRow;
    int currentRow;
    int prevGroup;
    int groupIndex;
    int isDisabled;
    int isPurchased;
    int maxLabelWidth;
    int blockPadding;
    int blockWidth;

    renderInitColors();
    terminalGetSize(&terminalWidth, &terminalHeight);
    if (isatty(STDOUT_FILENO) &&
        !renderTerminalSizeOk(&terminalWidth, &terminalHeight)) {
        renderTooSmallScreen(terminalWidth, terminalHeight);
        return;
    }

    tableWidth = getDynamicTableWidth(game);
    if (isatty(STDOUT_FILENO) &&
        terminalWidth >= MIN_TERMINAL_WIDTH &&
        terminalHeight >= MIN_TERMINAL_HEIGHT) {
        renderHideCursor();
        shopPadding = getCenteredColumn(tableWidth) - 1;
        printf("%s%s%s", scrReset(), SCR_CLEAR, SCR_HOME);
        renderScreenHeader("TTY21 - SHOP", NULL, ROW_SHOP_TITLE);
        moveToTableRow(ROW_SHOP_MARKS, shopPadding);
        printf("Marks: %d", game->run.marks);
        moveToTableRow(ROW_SHOP_CHIPS, shopPadding);
        printf("Chips: %d", game->player.chips);

        maxLabelWidth = 0;
        for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
            int width;

            width = (int)strlen(items[itemIndex].label);
            if (width > maxLabelWidth) {
                maxLabelWidth = width;
            }
        }
        blockWidth = 6 + maxLabelWidth;
        blockPadding = getCenteredColumn(blockWidth) - 1;

        currentRow = SHOP_ITEM_START_ROW;
        prevGroup = -1;
        for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
            if (groups[itemIndex] != prevGroup) {
                groupIndex = groups[itemIndex];
                if (groupIndex >= 0 && groupIndex < 3) {
                    printf(SCR_CURSOR, currentRow, blockPadding + 1);
                    printf(SCR_ERASE_EOL);
                    printf("%s%s%s",
                           scrMuted(),
                           GROUP_TITLES[groupIndex],
                           scrReset());
                    currentRow++;
                }
                prevGroup = groups[itemIndex];
            }
            itemRow = currentRow++;
            isDisabled = (disabledMask & (1 << itemIndex)) != 0;
            isPurchased = strncmp(items[itemIndex].label,
                                  "[ PURCHASED ]",
                                  sizeof("[ PURCHASED ]") - 1) == 0;

            renderMenuItemRow(itemRow,
                              blockPadding + 1,
                              items[itemIndex].hotkey,
                              items[itemIndex].label,
                              itemIndex == selectedIndex,
                              isDisabled,
                              isPurchased);
        }

        moveToTableRow(SHOP_DESCRIPTION_ROW, blockPadding);
        if (selectedIndex >= 0 && selectedIndex < itemCount &&
            descriptions[selectedIndex] != NULL) {
            printf(SCR_ERASE_EOL);
            printf("%s%s%s", scrMuted(), descriptions[selectedIndex], scrReset());
        } else {
            printf(SCR_ERASE_EOL);
        }
        moveToTableRow(SHOP_HINT_ROW, shopPadding);
        printf(SCR_ERASE_EOL);
        printf("%sKeys: 1-9/A-C perks; V convert; N net; T tip; L loan; R/B twists; P slot; Q close%s",
               scrMuted(), scrReset());
        renderEventLog(game, shopPadding, terminalWidth);
        fflush(stdout);
    } else {
        printf("TTY21 - SHOP\n");
        printf("Marks: %d | Chips: %d\n\n", game->run.marks, game->player.chips);
        prevGroup = -1;
        for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
            if (groups[itemIndex] != prevGroup) {
                groupIndex = groups[itemIndex];
                if (groupIndex >= 0 && groupIndex < 3) {
                    printf("\n%s\n", GROUP_TITLES[groupIndex]);
                }
                prevGroup = groups[itemIndex];
            }
            isDisabled = (disabledMask & (1 << itemIndex)) != 0;
            {
                char line[512];

                formatPlainMenuItem(line,
                                    sizeof(line),
                                    items[itemIndex].hotkey,
                                    items[itemIndex].label,
                                    itemIndex == selectedIndex,
                                    isDisabled);
                printf("%s\n", line);
            }
            if (itemIndex == selectedIndex && descriptions[itemIndex] != NULL) {
                printf("    %s\n", descriptions[itemIndex]);
            }
        }
        printf("Keys: 1-9/A-C perks; V convert; N net; T tip; L loan; R/B twists; P slot; Q close\n");
        fflush(stdout);
    }
}
