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
        colorEnabled = getenv("NO_COLOR") == NULL;
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
    int maxCards;

    maxCards = game->dealer.hand.count;

    if (game->player.hand.count > maxCards) {
        maxCards = game->player.hand.count;
    }

    if (maxCards < 5) {
        maxCards = 5;
    }

    return maxCards * CARD_WIDTH + (maxCards - 1) * CARD_GAP;
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
    char dealerName[64];
    char statusText[128];
    char rulesText[256];
    char twistText[256];
    char oracleText[64];
    char roundText[64];
    char dealerTotal[32];
    char playerTotal[32];

    initColors();

    dealerInfo = getDealerInfo(game->dealerId);
    twistInfo = getTwistInfo(game->twistId);
    tableWidth = getDynamicTableWidth(game);
    dealerCardHidden = hideDealerCard;

    if (game->twistId == TWIST_SNIPER) {
        dealerCardHidden = 0;
    }

    if (canRenderTable(tableWidth,
                       &terminalWidth,
                       &terminalHeight)) {
        tablePadding = (terminalWidth - tableWidth) / 2;

        snprintf(roundText,
                 sizeof(roundText),
                 "Round: %d",
                 game->roundNumber);

        if (dealerCardHidden || game->dealerId == DEALER_REAPER) {
            snprintf(dealerTotal, sizeof(dealerTotal), "Total: ??");
        } else {
            snprintf(dealerTotal,
                     sizeof(dealerTotal),
                     "Total: %d",
                     calculateDealerHandValue(&game->dealer.hand,
                                              game->dealerId));
        }

        snprintf(playerTotal,
                 sizeof(playerTotal),
                 "Total: %d",
                 game->player.hand.value);

        snprintf(statusText,
                 sizeof(statusText),
                 "Floor: %d    Dealer: %s    Souls: %d    Chips: %d    Bet: %d",
                 game->floor,
                 dealerInfo->name,
                 game->souls,
                 game->player.chips,
                 game->player.bet);

        if ((int)strlen(statusText) > tableWidth) {
            snprintf(dealerName,
                     sizeof(dealerName),
                     "%s",
                     dealerInfo->name);
            snprintf(statusText,
                     sizeof(statusText),
                     "Floor: %d    Dealer: ",
                     game->floor);
            prefixLength = (int)strlen(statusText);
            snprintf(statusText,
                     sizeof(statusText),
                     "    Souls: %d    Chips: %d    Bet: %d",
                     game->souls,
                     game->player.chips,
                     game->player.bet);
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

            snprintf(statusText,
                     sizeof(statusText),
                     "Floor: %d    Dealer: %s    Souls: %d    Chips: %d    Bet: %d",
                     game->floor,
                     dealerName,
                     game->souls,
                     game->player.chips,
                     game->player.bet);
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
        drawCards(&game->player.hand,
                  PLAYER_CARD_ROW,
                  tablePadding,
                  0);

        moveToTableRow(ROW_STATUS, tablePadding);
        printf("%s", statusText);

        if (game->state == STATE_BET) {
            hintText = "Type a bet and press Enter.";
        } else if (game->state == STATE_PLAYER_TURN) {
            hintText = "Keys: H hit   S stand   Q quit";
        } else if (game->state == STATE_MENU) {
            hintText = "Keys: C continue   A advance   Q quit";
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

        if (game->dealerId == DEALER_ORACLE &&
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
                int startRow) {
    int terminalWidth;
    int terminalHeight;
    int itemIndex;
    int menuPadding;
    int isDisabled;

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

            moveToTableRow(startRow + itemIndex, menuPadding);
            printf(SCR_ERASE_EOL);

            if (isDisabled) {
                printf("%s%s%s",
                       scrDim(),
                       items[itemIndex],
                       scrReset());
            } else {
                printf("%s", items[itemIndex]);
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
