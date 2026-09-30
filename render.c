#include "render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define TABLE_MIN_HEIGHT 40
#define CARD_WIDTH 11
#define CARD_GAP 1
#define CARD_CONTENT_WIDTH 9
#define CARD_HEIGHT 7

#define TITLE_ROW 1
#define SEPARATOR_ROW 2
#define ROUND_ROW 4
#define DEALER_LABEL_ROW 6
#define DEALER_CARD_ROW 8
#define PLAYER_LABEL_ROW 16
#define PLAYER_CARD_ROW 18

static const char *CARD_RANKS[] = {
    "", "A", "2", "3", "4", "5", "6",
    "7", "8", "9", "10", "J", "Q", "K"
};

static const char *CARD_SUITS[] = {
    "♥", "♦", "♣", "♠"
};

static int tablePadding;
static int colorEnabled = 1;

// Reads NO_COLOR once before the first visible render.
static void initColors(void) {
    static int initialized;

    if (!initialized) {
        colorEnabled = getenv("NO_COLOR") == NULL &&
                       isatty(STDOUT_FILENO);
        initialized = 1;
    }
}

// Returns the ANSI reset sequence when colors are enabled.
static const char *scrReset(void) {
    return colorEnabled ? "\033[0m" : "";
}

// Returns the ANSI red sequence when colors are enabled.
static const char *scrRed(void) {
    return colorEnabled ? "\033[31m" : "";
}

// Returns the ANSI green sequence when colors are enabled.
static const char *scrGreen(void) {
    return colorEnabled ? "\033[32m" : "";
}

// Returns the ANSI yellow sequence when colors are enabled.
static const char *scrYellow(void) {
    return colorEnabled ? "\033[33m" : "";
}

// Returns the ANSI bold sequence when colors are enabled.
static const char *scrBold(void) {
    return colorEnabled ? "\033[1m" : "";
}

// Returns the ANSI dim sequence when colors are enabled.
static const char *scrDim(void) {
    return colorEnabled ? "\033[2m" : "";
}

// Reads terminal dimensions and falls back to a standard terminal size.
static void getTerminalSize(int *width, int *height) {
    struct winsize windowSize;

    *width = 80;
    *height = 24;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &windowSize) == 0) {
        if (windowSize.ws_col > 0) {
            *width = windowSize.ws_col;
        }

        if (windowSize.ws_row > 0) {
            *height = windowSize.ws_row;
        }
    }
}

// Calculates the table width from the largest hand with a stable minimum.
static int getDynamicTableWidth(const Game *game) {
    int cardsTotal;
    int playerCardsTotal;
    int handIndex;

    cardsTotal = game->dealer.hand.count;
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

// Checks whether the dynamic table grid fits the current terminal.
static int canRenderTable(int tableWidth,
                          int *width,
                          int *height) {
    getTerminalSize(width, height);

    return isatty(STDOUT_FILENO) &&
           *width >= tableWidth + 2 &&
           *height >= TABLE_MIN_HEIGHT;
}

// Moves the cursor to a table row and left-aligns content with the table.
static void moveToTableRow(int row, int padding) {
    printf(SCR_CURSOR, row, padding + 1);
}

// Prints a centered line at a fixed row.
static void printCenteredAtRow(const char *text, int row, int width) {
    int textWidth;
    int padding;

    textWidth = (int)strlen(text);
    padding = (width - textWidth) / 2;

    if (padding < 0) {
        padding = 0;
    }

    printf(SCR_CURSOR, row, padding + 1);
    printf("%s", text);
}

// Prints the dealer or player label with its total.
static void printHandHeader(const char *label,
                            const char *totalText,
                            int row,
                            int padding,
                            int tableWidth) {
    int remainingWidth;

    remainingWidth = tableWidth -
                     (int)strlen(label) -
                     (int)strlen(totalText);

    if (remainingWidth < 1) {
        remainingWidth = 1;
    }

    moveToTableRow(row, padding);
    printf("%s%s%s%*s%s",
           scrBold(),
           label,
           scrReset(),
           remainingWidth,
           "",
           totalText);
}

// Prints one line from a face-down card.
static void printFaceDownLine(int lineIndex) {
    static const char *PATTERN_LINES[] = {
        "| # # # # |",
        "|  # # #  |",
        "| # # # # |",
        "|  # # #  |",
        "| # # # # |"
    };

    printf("%s", PATTERN_LINES[lineIndex]);
}

// Prints one line from a visible card.
static void printVisibleCardLine(const Card *card, int lineIndex) {
    const char *rankName;

    rankName = CARD_RANKS[card->rank];

    if (lineIndex == 0) {
        printf("|%-2s       |", rankName);
    } else if (lineIndex == 1 || lineIndex == 3) {
        printf("|%*s|", CARD_CONTENT_WIDTH, "");
    } else if (lineIndex == 2) {
        printf("|    ");

        if (card->suit == HEARTS || card->suit == DIAMONDS) {
            printf("%s%s%s",
                   scrRed(),
                   CARD_SUITS[card->suit],
                   scrReset());
        } else {
            printf("%s", CARD_SUITS[card->suit]);
        }

        printf("    |");
    } else {
        printf("|       %2s|", rankName);
    }
}

// Draws one complete row of cards.
static void drawCards(const Hand *hand,
                      int startRow,
                      int padding,
                      int hideFirstCard) {
    int lineIndex;
    int cardIndex;

    for (lineIndex = 0; lineIndex < CARD_HEIGHT; lineIndex++) {
        moveToTableRow(startRow + lineIndex, padding);

        for (cardIndex = 0; cardIndex < hand->count; cardIndex++) {
            if (lineIndex == 0 || lineIndex == CARD_HEIGHT - 1) {
                printf("+---------+");
            } else if (hideFirstCard && cardIndex == 0) {
                printFaceDownLine(lineIndex - 1);
            } else {
                printVisibleCardLine(&hand->cards[cardIndex], lineIndex - 1);
            }

            if (cardIndex < hand->count - 1) {
                printf(" ");
            }
        }
    }
}

// Prints the status below one player hand.
static void printPlayerHandStatus(const Game *game,
                                  int handIndex,
                                  int padding) {
    int handWidth;
    int handValue;
    int textWidth;
    int textPadding;
    const char *style;
    char statusText[32];

    handWidth = game->player.hands[handIndex].count * CARD_WIDTH +
                (game->player.hands[handIndex].count - 1) * CARD_GAP;

    if (handIndex == game->player.activeHand &&
        game->player.activeHand < game->player.handCount) {
        handValue = calculatePlayerHandValue(game);
    } else {
        handValue = game->player.hands[handIndex].value;
    }

    if (handValue > 21) {
        snprintf(statusText, sizeof(statusText), "BUST");
    } else if (game->player.hands[handIndex].count == 2 &&
               handValue == 21) {
        snprintf(statusText, sizeof(statusText), "BJ");
    } else {
        snprintf(statusText, sizeof(statusText), "%d", handValue);
    }

    textWidth = (int)strlen(statusText);
    textPadding = (handWidth - textWidth) / 2;

    if (textPadding < 0) {
        textPadding = 0;
    }

    style = handIndex == game->player.activeHand &&
            game->player.activeHand < game->player.handCount
                ? scrBold()
                : scrDim();

    moveToTableRow(PLAYER_CARD_ROW + CARD_HEIGHT, padding);
    printf("%*s%s%s%s",
           textPadding,
           "",
           style,
           statusText,
           scrReset());
}

// Draws all player hands side by side with two spaces between hands.
static void drawPlayerHands(const Game *game,
                            int padding) {
    int handIndex;
    int handPadding;
    int handWidth;

    handPadding = padding;

    for (handIndex = 0;
         handIndex < game->player.handCount;
         handIndex++) {
        drawCards(&game->player.hands[handIndex],
                  PLAYER_CARD_ROW,
                  handPadding,
                  0);

        handWidth = game->player.hands[handIndex].count * CARD_WIDTH +
                    (game->player.hands[handIndex].count - 1) * CARD_GAP;
        handPadding += handWidth + 2;
    }

    handPadding = padding;

    for (handIndex = 0;
         handIndex < game->player.handCount;
         handIndex++) {
        moveToTableRow(PLAYER_CARD_ROW - 1, handPadding);

        if (handIndex == game->player.activeHand &&
            game->player.activeHand < game->player.handCount) {
            printf("> ");
        } else {
            printf("  ");
        }

        printPlayerHandStatus(game, handIndex, handPadding);

        handWidth = game->player.hands[handIndex].count * CARD_WIDTH +
                    (game->player.hands[handIndex].count - 1) * CARD_GAP;
        handPadding += handWidth + 2;
    }
}

// Clears and redraws the complete table on fixed rows.
void renderTable(const Game *game, int hideDealerCard) {
    const DealerInfo *dealerInfo;
    const TwistInfo *twistInfo;
    const char *hintText;
    int terminalWidth;
    int terminalHeight;
    int tableWidth;
    int dealerCardHidden;
    int separatorIndex;
    int prefixLength;
    int suffixLength;
    int availableNameWidth;
    int displayHandIndex;
    int displayHandValue;
    char dealerName[64];
    char statusText[256];
    char rulesText[256];
    char twistText[256];
    char oracleText[64];
    char roundText[64];
    char dealerTotal[32];
    char playerTotal[32];

    initColors();

    dealerInfo = getDealerInfo(game->run.dealerId);
    twistInfo = getTwistInfo(game->run.twistId);
    tableWidth = getDynamicTableWidth(game);
    dealerCardHidden = hideDealerCard &&
                       !game->peekActive &&
                       game->run.twistId != TWIST_SNIPER;

    displayHandIndex = game->player.activeHand;

    if (displayHandIndex >= game->player.handCount) {
        displayHandIndex = game->player.handCount - 1;
    }

    if (displayHandIndex < 0) {
        displayHandIndex = 0;
    }

    if (canRenderTable(tableWidth,
                       &terminalWidth,
                       &terminalHeight)) {
        tablePadding = (terminalWidth - tableWidth) / 2;

        snprintf(roundText,
                 sizeof(roundText),
                 "Round: %d",
                 game->roundNumber);

        if (dealerCardHidden || game->run.dealerId == DEALER_REAPER) {
            snprintf(dealerTotal, sizeof(dealerTotal), "Total: ??");
        } else {
            snprintf(dealerTotal,
                     sizeof(dealerTotal),
                     "Total: %d",
                     calculateDealerHandValue(&game->dealer.hand,
                                              game->run.dealerId));
        }

        if (game->player.activeHand < game->player.handCount) {
            displayHandValue = calculatePlayerHandValue(game);
        } else {
            displayHandValue = game->player.hands[displayHandIndex].value;
        }

        snprintf(playerTotal,
                 sizeof(playerTotal),
                 "Total: %d",
                 displayHandValue);

        if (game->player.handCount == 1) {
            snprintf(statusText,
                     sizeof(statusText),
                     "Floor: %d    Dealer: %s    Marks: %d    Chips: %d    Bet: %d",
                     game->run.floor,
                     dealerInfo->name,
                     game->run.souls,
                     game->player.chips,
                     game->player.bets[displayHandIndex]);
        } else {
            snprintf(statusText,
                     sizeof(statusText),
                     "Floor: %d    Dealer: %s    Marks: %d    Chips: %d    Bet: %d (hand %d/%d)",
                     game->run.floor,
                     dealerInfo->name,
                     game->run.souls,
                     game->player.chips,
                     game->player.bets[displayHandIndex],
                     displayHandIndex + 1,
                     game->player.handCount);
        }

        if ((int)strlen(statusText) > tableWidth) {
            snprintf(dealerName,
                     sizeof(dealerName),
                     "%s",
                     dealerInfo->name);
            snprintf(statusText,
                     sizeof(statusText),
                     "Floor: %d    Dealer: ",
                     game->run.floor);
            prefixLength = (int)strlen(statusText);

            if (game->player.handCount == 1) {
                snprintf(statusText,
                         sizeof(statusText),
                         "    Marks: %d    Chips: %d    Bet: %d",
                         game->run.souls,
                         game->player.chips,
                         game->player.bets[displayHandIndex]);
            } else {
                snprintf(statusText,
                         sizeof(statusText),
                         "    Marks: %d    Chips: %d    Bet: %d (hand %d/%d)",
                         game->run.souls,
                         game->player.chips,
                         game->player.bets[displayHandIndex],
                         displayHandIndex + 1,
                         game->player.handCount);
            }

            suffixLength = (int)strlen(statusText);
            availableNameWidth = tableWidth - prefixLength - suffixLength;

            if (availableNameWidth < 2) {
                availableNameWidth = 2;
            }

            if ((int)strlen(dealerName) > availableNameWidth) {
                dealerName[availableNameWidth - 2] = '.';
                dealerName[availableNameWidth - 1] = '.';
                dealerName[availableNameWidth] = '\0';
            }

            if (game->player.handCount == 1) {
                snprintf(statusText,
                         sizeof(statusText),
                         "Floor: %d    Dealer: %s    Marks: %d    Chips: %d    Bet: %d",
                         game->run.floor,
                         dealerName,
                         game->run.souls,
                         game->player.chips,
                         game->player.bets[displayHandIndex]);
            } else {
                snprintf(statusText,
                         sizeof(statusText),
                         "Floor: %d    Dealer: %s    Marks: %d    Chips: %d    Bet: %d (hand %d/%d)",
                         game->run.floor,
                         dealerName,
                         game->run.souls,
                         game->player.chips,
                         game->player.bets[displayHandIndex],
                         displayHandIndex + 1,
                         game->player.handCount);
            }
        }

        printf("%s%s%s", scrReset(), SCR_CLEAR, SCR_HOME);

        printf(SCR_CURSOR,
               TITLE_ROW,
               (terminalWidth - (int)strlen("TTY21")) / 2 + 1);
        printf("%sTTY21%s", scrBold(), scrReset());

        moveToTableRow(SEPARATOR_ROW, tablePadding);

        for (separatorIndex = 0;
             separatorIndex < tableWidth;
             separatorIndex++) {
            printf("-");
        }

        printCenteredAtRow(roundText, ROUND_ROW, terminalWidth);
        printHandHeader("DEALER",
                        dealerTotal,
                        DEALER_LABEL_ROW,
                        tablePadding,
                        tableWidth);
        drawCards(&game->dealer.hand,
                  DEALER_CARD_ROW,
                  tablePadding,
                  dealerCardHidden);

        printHandHeader("PLAYER",
                        playerTotal,
                        PLAYER_LABEL_ROW,
                        tablePadding,
                        tableWidth);
        drawPlayerHands(game, tablePadding);

        moveToTableRow(ROW_STATUS, tablePadding);
        printf("%s", statusText);

        if (game->state == STATE_BET) {
            hintText = "Type a bet and press Enter.";
        } else if (game->state == STATE_PLAYER_TURN) {
            hintText = "";
        } else if (game->state == STATE_MENU) {
            hintText = "Keys: C continue   A advance   S shop   Q quit";
        } else {
            hintText = "";
        }

        moveToTableRow(ROW_HINT, tablePadding);
        printf(SCR_ERASE_EOL);

        if (hintText[0] != '\0') {
            printf("%s%s%s", scrDim(), hintText, scrReset());
        }

        moveToTableRow(ROW_RULE, tablePadding);
        printf(SCR_ERASE_EOL);

        if (dealerInfo->rule1 != NULL) {
            if (dealerInfo->rule2 == NULL) {
                snprintf(rulesText,
                         sizeof(rulesText),
                         "Rule: %s",
                         dealerInfo->rule1);
            } else {
                snprintf(rulesText,
                         sizeof(rulesText),
                         "Rules: %s | %s",
                         dealerInfo->rule1,
                         dealerInfo->rule2);
            }

            printf("%s%s%s", scrDim(), rulesText, scrReset());
        }

        moveToTableRow(ROW_TWIST, tablePadding);
        printf(SCR_ERASE_EOL);

        if (twistInfo->id != TWIST_NONE) {
            snprintf(twistText,
                     sizeof(twistText),
                     "Twist: %s — %s",
                     twistInfo->name,
                     twistInfo->description);
            printf("%s%s%s",
                   scrDim(),
                   twistText,
                   scrReset());
        }

        moveToTableRow(ROW_ORACLE, tablePadding);
        printf(SCR_ERASE_EOL);

        if (game->run.dealerId == DEALER_ORACLE &&
            game->state == STATE_PLAYER_TURN &&
            game->deck.top < DECK_SIZE) {
            snprintf(oracleText,
                     sizeof(oracleText),
                     "Next card: %s%s",
                     CARD_RANKS[game->deck.cards[game->deck.top].rank],
                     CARD_SUITS[game->deck.cards[game->deck.top].suit]);
            printf("%s%s%s", scrDim(), oracleText, scrReset());
        }

        fflush(stdout);
    } else {
        printGameState(game, hideDealerCard);
    }
}

// Prints a fixed menu block with disabled entries dimmed.
void renderMenu(const char *title,
                const char *const *items,
                int itemCount,
                int disabledMask,
                int selectedIndex,
                int startRow) {
    int terminalWidth;
    int terminalHeight;
    int itemIndex;
    int menuPadding;
    int isDisabled;
    const char *prefix;

    initColors();
    getTerminalSize(&terminalWidth, &terminalHeight);

    if (isatty(STDOUT_FILENO) &&
        terminalWidth >= 72 &&
        terminalHeight >= TABLE_MIN_HEIGHT) {
        menuPadding = tablePadding;

        printf(SCR_CURSOR, startRow - 1, menuPadding + 1);
        printf(SCR_ERASE_EOL);
        printf("%s%s%s", scrBold(), title, scrReset());

        for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
            isDisabled = (disabledMask & (1 << itemIndex)) != 0;
            prefix = itemIndex == selectedIndex && !isDisabled ? "> " : "  ";

            moveToTableRow(startRow + itemIndex, menuPadding);
            printf(SCR_ERASE_EOL);

            if (isDisabled) {
                printf("%s%s%s%s",
                       scrDim(),
                       prefix,
                       items[itemIndex],
                       scrReset());
            } else if (itemIndex == selectedIndex) {
                printf("%s%s%s%s",
                       scrBold(),
                       prefix,
                       items[itemIndex],
                       scrReset());
            } else {
                printf("%s%s", prefix, items[itemIndex]);
            }
        }

        fflush(stdout);
    } else {
        printf("%s\n", title);

        for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
            isDisabled = (disabledMask & (1 << itemIndex)) != 0;

            if (isDisabled) {
                printf("%s%s%s",
                       scrDim(),
                       items[itemIndex],
                       scrReset());
            } else {
                printf("%s", items[itemIndex]);
            }

            printf("\n");
        }

        fflush(stdout);
    }
}

// Renders the between-floor perk shop.
void renderShop(const Game *game, int selectedIndex) {
    int terminalWidth;
    int terminalHeight;
    int tableWidth;
    int shopPadding;
    int titlePadding;
    int itemRow;
    int descriptionRow;
    int perkIndex;
    int isOwned;
    int canAfford;
    const char *statusText;

    static const int ITEM_ROWS[] = {
        ROW_SHOP_ITEM_0,
        ROW_SHOP_ITEM_1,
        ROW_SHOP_ITEM_2,
        ROW_SHOP_ITEM_3,
        ROW_SHOP_ITEM_4
    };

    static const int DESCRIPTION_ROWS[] = {
        ROW_SHOP_DESC_0,
        ROW_SHOP_DESC_1,
        ROW_SHOP_DESC_2,
        ROW_SHOP_DESC_3,
        ROW_SHOP_DESC_4
    };

    initColors();
    getTerminalSize(&terminalWidth, &terminalHeight);
    tableWidth = getDynamicTableWidth(game);

    if (isatty(STDOUT_FILENO) &&
        terminalWidth >= 72 &&
        terminalHeight >= TABLE_MIN_HEIGHT) {
        shopPadding = (terminalWidth - tableWidth) / 2;

        if (shopPadding < 0) {
            shopPadding = 0;
        }

        titlePadding = shopPadding +
                        (tableWidth - (int)strlen("SHOP")) / 2;

        printf("%s%s%s", scrReset(), SCR_CLEAR, SCR_HOME);
        printf(SCR_CURSOR, ROW_SHOP_TITLE, titlePadding + 1);
        printf("%sSHOP%s", scrBold(), scrReset());

        moveToTableRow(ROW_SHOP_SOULS, shopPadding);
        printf("Souls: %d", game->run.souls);

        for (perkIndex = 0; perkIndex < PERK_COUNT; perkIndex++) {
            itemRow = ITEM_ROWS[perkIndex];
            descriptionRow = DESCRIPTION_ROWS[perkIndex];
            isOwned = game->player.perkOwned[perkIndex];
            canAfford = game->run.souls >= perkCost((PerkId)perkIndex);
            statusText = isOwned ? "OWNED" : "BUY";

            moveToTableRow(itemRow, shopPadding);

            if (perkIndex == selectedIndex) {
                printf("%s> ", scrBold());
            } else if (!isOwned && !canAfford) {
                printf("%s  ", scrDim());
            } else {
                printf("  ");
            }

            printf("[%d] %s   %d souls   %s",
                   perkIndex + 1,
                   perkName((PerkId)perkIndex),
                   perkCost((PerkId)perkIndex),
                   statusText);
            printf("%s", scrReset());

            moveToTableRow(descriptionRow, shopPadding);
            printf("%s%s%s",
                   scrDim(),
                   perkDescription((PerkId)perkIndex),
                   scrReset());
        }

        moveToTableRow(ROW_SHOP_HINT, shopPadding);
        printf("%sKeys: 1-5 buy   Q close%s",
               scrDim(),
               scrReset());
        fflush(stdout);
    } else {
        printf("SHOP\n");
        printf("Souls: %d\n\n", game->run.souls);

        for (perkIndex = 0; perkIndex < PERK_COUNT; perkIndex++) {
            isOwned = game->player.perkOwned[perkIndex];
            canAfford = game->run.souls >= perkCost((PerkId)perkIndex);
            statusText = isOwned ? "OWNED" : "BUY";

            if (!isOwned && !canAfford) {
                printf("%s[%d] %s   %d souls   %s%s",
                       scrDim(),
                       perkIndex + 1,
                       perkName((PerkId)perkIndex),
                       perkCost((PerkId)perkIndex),
                       statusText,
                       scrReset());
            } else {
                printf("[%d] %s   %d souls   %s",
                       perkIndex + 1,
                       perkName((PerkId)perkIndex),
                       perkCost((PerkId)perkIndex),
                       statusText);
            }

            printf("\n%s%s%s\n",
                   scrDim(),
                   perkDescription((PerkId)perkIndex),
                   scrReset());
        }

        printf("%sKeys: 1-5 buy   Q close%s\n",
               scrDim(),
               scrReset());
        fflush(stdout);
    }
}

// Prints a message on a fixed row after erasing the rest of that line.
void renderMessage(const char *msg, int row, MessageStyle style) {
    const char *prefix;
    const char *suffix;
    int terminalWidth;
    int terminalHeight;
    int messagePadding;

    initColors();
    prefix = "";
    suffix = "";

    if (style == MSG_WIN) {
        prefix = scrGreen();
        suffix = scrReset();
    } else if (style == MSG_LOSS) {
        prefix = scrRed();
        suffix = scrReset();
    } else if (style == MSG_PUSH) {
        prefix = scrYellow();
        suffix = scrReset();
    } else if (style == MSG_INFO) {
        prefix = scrDim();
        suffix = scrReset();
    }

    getTerminalSize(&terminalWidth, &terminalHeight);
    messagePadding = tablePadding;

    if (isatty(STDOUT_FILENO) &&
        terminalWidth >= 72 &&
        terminalHeight >= TABLE_MIN_HEIGHT) {
        printf(SCR_CURSOR, row, messagePadding + 1);
        printf(SCR_ERASE_EOL);
        printf("%s%s%s", prefix, msg, suffix);
        fflush(stdout);
    } else {
        printf("%s%s%s\n", prefix, msg, suffix);
        fflush(stdout);
    }
}

 
// Renders the horizontal player action bar.
void renderActionBar(const ActionId *actions,
                     int actionCount,
                     int selectedIndex,
                     int row) {
    static const char *ACTION_NAMES[] = {
        "Hit",
        "Stand",
        "Double",
        "Split",
        "Peek",
        "Quit"
    };
    int actionIndex;
    int terminalWidth;
    int terminalHeight;

    getTerminalSize(&terminalWidth, &terminalHeight);

    if (!isatty(STDOUT_FILENO) ||
        terminalWidth < 72 ||
        terminalHeight < TABLE_MIN_HEIGHT ||
        actionCount <= 0 ||
        selectedIndex < 0 ||
        selectedIndex >= actionCount) {
        return;
    }

    printf(SCR_CURSOR, row, tablePadding + 1);
    printf(SCR_ERASE_EOL);
    printf("%s", scrDim());

    for (actionIndex = 0; actionIndex < actionCount; actionIndex++) {
        if (actionIndex > 0) {
            printf("   ");
        }

        if (actionIndex == selectedIndex) {
            printf("%s> %s%s",
                   scrBold(),
                   ACTION_NAMES[actions[actionIndex]],
                   scrDim());
        } else {
            printf("  %s", ACTION_NAMES[actions[actionIndex]]);
        }
    }

    printf("  ");
    printf("%s", scrReset());
    fflush(stdout);
}
