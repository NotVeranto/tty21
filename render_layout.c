#include "render_layout.h"
#include "render_style.h"
#include "terminal.h"
#include "render_cards.h"

#include <stdio.h>
#include <string.h>

int getCenteredColumn(int contentWidth) {
    int terminalWidth;
    int terminalHeight;
    int column;

    terminalGetSize(&terminalWidth, &terminalHeight);
    (void)terminalHeight;

    column = (terminalWidth - contentWidth) / 2 + 1;
    if (column < 1) {
        column = 1;
    }

    return column;
}

void printBoxBorder(int row, int column, int width) {
    int index;

    printf(SCR_CURSOR, row, column);
    printf("%s+", scrDim());
    for (index = 0; index < width - 2; index++) {
        putchar('-');
    }
    printf("+%s", scrReset());
}

void printBoxSides(int row, int column, int width) {
    printf(SCR_CURSOR, row, column);
    printf("%s|%s", scrDim(), scrReset());
    printf(SCR_CURSOR, row, column + width - 1);
    printf("%s|%s", scrDim(), scrReset());
}

int getDynamicTableWidth(const Game *game) {
    int cardsTotal;
    int playerCardsTotal;
    int handIndex;
    int dealerHandIndex;
    int totalDealerCards;

    totalDealerCards = 0;

    for (dealerHandIndex = 0;
         dealerHandIndex < game->dealer.handCount;
         dealerHandIndex++) {
        totalDealerCards += game->dealer.hands[dealerHandIndex].count;
    }

    if (game->dealer.handCount > 1) {
        totalDealerCards += (game->dealer.handCount - 1) * 2;
    }

    cardsTotal = totalDealerCards;
    playerCardsTotal = 0;

    for (handIndex = 0;
         handIndex < game->player.handCount;
         handIndex++) {
        playerCardsTotal += game->player.hands[handIndex].count;
    }

    if (game->player.handCount > 1) {
        playerCardsTotal += (game->player.handCount - 1) * 2;
    }

    if (playerCardsTotal > cardsTotal) {
        cardsTotal = playerCardsTotal;
    }

    if (cardsTotal < 5) {
        cardsTotal = 5;
    }

    return cardsTotal * CARD_WIDTH + (cardsTotal - 1) * CARD_GAP;
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
                           scrDim(),
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

            printf(SCR_CURSOR, itemRow, blockPadding + 1);
            printf(SCR_ERASE_EOL);
            if (isPurchased) {
                if (itemIndex == selectedIndex) {
                    printf("%s> %s[%c]%s %s%s%s",
                           scrBold(), scrDim(), items[itemIndex].hotkey,
                           scrReset(), scrGreen(), items[itemIndex].label,
                           scrReset());
                } else {
                    printf("  %s[%c]%s %s%s%s",
                           scrDim(), items[itemIndex].hotkey, scrReset(),
                           scrGreen(), items[itemIndex].label, scrReset());
                }
            } else if (isDisabled) {
                printf("  %s[%c]%s %s%s%s",
                       scrDim(), items[itemIndex].hotkey, scrReset(),
                       scrDim(), items[itemIndex].label, scrReset());
            } else if (itemIndex == selectedIndex) {
                printf("%s> %s[%c]%s %s%s%s",
                       scrBold(), scrDim(), items[itemIndex].hotkey,
                       scrReset(), scrBold(), items[itemIndex].label,
                       scrReset());
            } else {
                printf("  %s[%c]%s %s",
                       scrDim(), items[itemIndex].hotkey, scrReset(),
                       items[itemIndex].label);
            }
        }

        moveToTableRow(SHOP_DESCRIPTION_ROW, blockPadding);
        if (selectedIndex >= 0 && selectedIndex < itemCount &&
            descriptions[selectedIndex] != NULL) {
            printf(SCR_ERASE_EOL);
            printf("%s%s%s", scrDim(), descriptions[selectedIndex], scrReset());
        } else {
            printf(SCR_ERASE_EOL);
        }
        moveToTableRow(SHOP_HINT_ROW, shopPadding);
        printf(SCR_ERASE_EOL);
        printf("%sKeys: 1-9/A-C perks; V convert; N net; T tip; L loan; R/B twists; P slot; Q close%s",
               scrDim(), scrReset());
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
            printf("%s[%c] %s%s\n",
                   itemIndex == selectedIndex ? "> " : "  ",
                   items[itemIndex].hotkey,
                   items[itemIndex].label,
                   isDisabled ? " (disabled)" : "");
            if (itemIndex == selectedIndex && descriptions[itemIndex] != NULL) {
                printf("    %s\n", descriptions[itemIndex]);
            }
        }
        printf("Keys: 1-9/A-C perks; V convert; N net; T tip; L loan; R/B twists; P slot; Q close\n");
        fflush(stdout);
    }
}
