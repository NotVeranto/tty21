#include "render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define TABLE_MIN_HEIGHT MIN_TERMINAL_HEIGHT
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
#define SHOP_ITEM_START_ROW 13
#define SHOP_DESCRIPTION_ROW 30
#define SHOP_HINT_ROW 31

static const char *CARD_RANKS[] = {
    "", "A", "2", "3", "4", "5", "6",
    "7", "8", "9", "10", "J", "Q", "K"
};

static const char *CARD_SUITS[] = {
    "♥", "♦", "♣", "♠"
};

static int tablePadding;
static int colorEnabled = 1;
static int cursorHidden;

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

// Hides the hardware cursor while full-screen rendering is active.
void renderHideCursor(void) {
    initColors();

    if (colorEnabled && !cursorHidden) {
        printf("\033[?25l");
        fflush(stdout);
        cursorHidden = 1;
    }
}

// Restores the hardware cursor after full-screen rendering.
void renderShowCursor(void) {
    initColors();

    if (colorEnabled && cursorHidden) {
        printf("\033[?25h");
        fflush(stdout);
        cursorHidden = 0;
    }
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

// Returns the ANSI cyan sequence when colors are enabled.
static const char *scrCyan(void) {
    return colorEnabled ? "\033[36m" : "";
}

// Returns the ANSI magenta sequence when colors are enabled.
static const char *scrMagenta(void) {
    return colorEnabled ? "\033[35m" : "";
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

int renderTerminalSizeOk(int *width, int *height) {
    getTerminalSize(width, height);
    return *width >= MIN_TERMINAL_WIDTH &&
           *height >= MIN_TERMINAL_HEIGHT;
}

void renderTooSmallScreen(int currentWidth,
                          int currentHeight) {
    const char *msg;
    char sized[128];
    char current[32];
    int lines;
    int row;

    if (currentWidth <= 0 || currentHeight <= 0) {
        fprintf(stderr,
                "error: terminal too small.\n"
                "tty21 needs at least %dx%d.\n",
                MIN_TERMINAL_WIDTH, MIN_TERMINAL_HEIGHT);
        return;
    }

    if (currentWidth >= 40 && currentHeight >= 6) {
        msg =
            "Terminal too small.\n"
            "\n"
            "Current: %dx%d\n"
            "Minimum: %dx%d\n"
            "\n"
            "Resize the window to continue.";
        snprintf(sized, sizeof(sized), msg,
                 currentWidth, currentHeight,
                 MIN_TERMINAL_WIDTH, MIN_TERMINAL_HEIGHT);
        lines = 6;
    } else if (currentWidth >= 24 && currentHeight >= 3) {
        snprintf(sized, sizeof(sized),
                 "Need %dx%d (got %dx%d)",
                 MIN_TERMINAL_WIDTH, MIN_TERMINAL_HEIGHT,
                 currentWidth, currentHeight);
        lines = 1;
    } else {
        snprintf(current, sizeof(current),
                 "%dx%d", currentWidth, currentHeight);
        msg = current;
        snprintf(sized, sizeof(sized), "%s", msg);
        lines = 1;
    }

    if (colorEnabled && isatty(STDOUT_FILENO)) {
        printf("%s%s", SCR_CLEAR, SCR_HOME);
    }

    row = (currentHeight - lines) / 2;
    if (row < 1) {
        row = 1;
    }

    {
        /* Split sized on '\n' and center each line. */
        char *cursor;
        char *start;
        int lineNo;

        cursor = sized;
        lineNo = 0;

        while (*cursor != '\0' &&
               lineNo < lines) {
            char *nl;
            int len;
            int pad;

            start = cursor;
            nl = strchr(cursor, '\n');

            if (nl != NULL) {
                *nl = '\0';
            }

            len = (int)strlen(start);

            if (len > currentWidth) {
                start[currentWidth] = '\0';
                len = currentWidth;
            }

            pad = (currentWidth - len) / 2;
            if (pad < 0) {
                pad = 0;
            }

            if (colorEnabled && isatty(STDOUT_FILENO)) {
                printf(SCR_CURSOR, row + lineNo, pad + 1);
            } else {
                int k;
                for (k = 0; k < pad; k++) {
                    putchar(' ');
                }
            }

            printf("%s", start);

            if (!(colorEnabled && isatty(STDOUT_FILENO))) {
                putchar('\n');
            }

            lineNo++;

            if (nl == NULL) {
                break;
            }

            cursor = nl + 1;
        }
    }

    fflush(stdout);
}

// Calculates the table width from the largest hand with a stable minimum.
static int getDynamicTableWidth(const Game *game) {
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

// Draws all dealer hands side by side with two spaces between hands.
static void drawDealerHands(const Game *game,
                            int padding,
                            int hideFirstCard) {
    int handIndex;
    int handPadding;
    int handWidth;

    handPadding = padding;

    if (game->dealer.handCount > 1) {
        for (handIndex = 0;
             handIndex < game->dealer.handCount;
             handIndex++) {
            moveToTableRow(DEALER_CARD_ROW - 1, handPadding);
            printf("%sHAND %d%s",
                   scrDim(),
                   handIndex + 1,
                   scrReset());

            handWidth = game->dealer.hands[handIndex].count * CARD_WIDTH +
                        (game->dealer.hands[handIndex].count - 1) * CARD_GAP;
            handPadding += handWidth + 2;
        }
    }

    handPadding = padding;

    for (handIndex = 0;
         handIndex < game->dealer.handCount;
         handIndex++) {
        drawCards(&game->dealer.hands[handIndex],
                  DEALER_CARD_ROW,
                  handPadding,
                  hideFirstCard);

        handWidth = game->dealer.hands[handIndex].count * CARD_WIDTH +
                    (game->dealer.hands[handIndex].count - 1) * CARD_GAP;
        handPadding += handWidth + 2;
    }
}

// Prints the status below one player hand.
static void printPlayerHandStatus(const Game *game,
                                  int handIndex,
                                  int padding) {
    if (game->player.hands[handIndex].count == 0) {
        return;
    }

    int handWidth;
    int handValue;
    int textWidth;
    int textPadding;
    const char *style;
    char statusText[32];

    handWidth = game->player.hands[handIndex].count * CARD_WIDTH +
                (game->player.hands[handIndex].count - 1) * CARD_GAP;

    handValue =
        calculatePlayerHandValueForIndex(game, handIndex);

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
    int isActive;

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
        isActive = handIndex == game->player.activeHand &&
                   game->player.activeHand < game->player.handCount;

        moveToTableRow(PLAYER_CARD_ROW - 1, handPadding);

        if (game->player.handCount > 1) {
            if (isActive) {
                printf("%s> HAND %d%s",
                       scrBold(),
                       handIndex + 1,
                       scrReset());
            } else {
                printf("%s  HAND %d%s",
                       scrDim(),
                       handIndex + 1,
                       scrReset());
            }
        }

        printPlayerHandStatus(game, handIndex, handPadding);

        handWidth = game->player.hands[handIndex].count * CARD_WIDTH +
                    (game->player.hands[handIndex].count - 1) * CARD_GAP;
        handPadding += handWidth + 2;
    }
}

// Renders the persistent event log from oldest to newest.
static void renderLog(const Game *game,
                      int padding,
                      int terminalWidth) {
    int rowIndex;
    int sourceIndex;
    int maxWidth;
    int cut;
    const char *prefix;
    const char *suffix;
    const char *displayText;
    char truncated[LOG_LINE_LEN];

    maxWidth = terminalWidth - padding - 2;

    if (maxWidth < 8) {
        maxWidth = 8;
    }

    if (game->log.count == 0) {
        for (rowIndex = 0; rowIndex < LOG_SIZE; rowIndex++) {
            moveToTableRow(ROW_LOG_0 + rowIndex, padding);
            printf(SCR_ERASE_EOL);
        }

        fflush(stdout);
        return;
    }

    for (rowIndex = 0; rowIndex < LOG_SIZE; rowIndex++) {
        moveToTableRow(ROW_LOG_0 + rowIndex, padding);
        printf(SCR_ERASE_EOL);

        sourceIndex = rowIndex - (LOG_SIZE - game->log.count);

        if (sourceIndex < 0) {
            continue;
        }

        displayText = game->log.lines[sourceIndex];

        if ((int)strlen(displayText) > maxWidth) {
            cut = maxWidth - 3;

            if (cut < 1) {
                cut = 1;
            }

            if (cut > LOG_LINE_LEN - 4) {
                cut = LOG_LINE_LEN - 4;
            }

            memcpy(truncated, displayText, (size_t)cut);
            truncated[cut] = '.';
            truncated[cut + 1] = '.';
            truncated[cut + 2] = '.';
            truncated[cut + 3] = '\0';
            displayText = truncated;
        }

        prefix = "";
        suffix = "";

        if (game->log.styles[sourceIndex] == MSG_WIN) {
            prefix = scrGreen();
            suffix = scrReset();
        } else if (game->log.styles[sourceIndex] == MSG_LOSS) {
            prefix = scrRed();
            suffix = scrReset();
        } else if (game->log.styles[sourceIndex] == MSG_PUSH) {
            prefix = scrYellow();
            suffix = scrReset();
        } else if (game->log.styles[sourceIndex] == MSG_INFO) {
            prefix = scrDim();
            suffix = scrReset();
        } else if (game->log.styles[sourceIndex] == MSG_ACTION) {
            prefix = scrCyan();
            suffix = scrReset();
        } else if (game->log.styles[sourceIndex] == MSG_DEALER) {
            prefix = scrMagenta();
            suffix = scrReset();
        }

        printf("%s%s%s", prefix, displayText, suffix);
    }

    fflush(stdout);
}

// Clears and redraws the complete table on fixed rows.
void renderTable(const Game *game, int hideDealerCard) {
    const DealerInfo *dealerInfo;
    const TwistInfo *twistInfo;
    const char *hintText;
    const char *phaseText;
    int terminalWidth;
    int terminalHeight;
    int tableWidth;
    int dealerCardHidden;
    int separatorIndex;
    int prefixLength;
    int suffixLength;
    int availableNameWidth;
    int effectiveStatusWidth;
    int displayHandIndex;
    int displayHandValue;
    int dealerValue;
    int i;
    int len;
    char dealerName[64];
    char statusText[256];
    char rulesText[256];
    char twistsLine[256];
    char oracleText[256];
    char roundText[64];
    char dealerTotal[32];
    char playerTotal[32];

    initColors();

    dealerInfo = getDealerInfo(game->run.dealerId);
    twistInfo = getTwistInfo(game->run.twistId);
    tableWidth = getDynamicTableWidth(game);
    dealerCardHidden = hideDealerCard &&
                       !game->peekActive &&
                       !twistActive(game, TWIST_SNIPER);

    displayHandIndex = game->player.activeHand;

    if (displayHandIndex >= game->player.handCount) {
        displayHandIndex = game->player.handCount - 1;
    }

    if (displayHandIndex < 0) {
        displayHandIndex = 0;
    }

    getTerminalSize(&terminalWidth, &terminalHeight);

    if (isatty(STDOUT_FILENO) &&
        !renderTerminalSizeOk(&terminalWidth,
                              &terminalHeight)) {
        renderTooSmallScreen(terminalWidth, terminalHeight);
        return;
    }

    if (isatty(STDOUT_FILENO) &&
        terminalWidth >= tableWidth + 2) {
        renderHideCursor();
        tablePadding = (terminalWidth - tableWidth) / 2;

        phaseText = "";

        switch (game->state) {
            case STATE_BET:
                phaseText = "[ BET ]";
                break;
            case STATE_PLAYER_TURN:
                phaseText = "[ PLAYER TURN ]";
                break;
            case STATE_DEALER_TURN:
                phaseText = "[ DEALER TURN ]";
                break;
            case STATE_RESULT:
                phaseText = "[ RESULT ]";
                break;
            default:
                break;
        }

        if (phaseText[0] == '\0') {
            snprintf(roundText,
                     sizeof(roundText),
                     "Round: %d",
                     game->roundNumber);
        } else {
            snprintf(roundText,
                     sizeof(roundText),
                     "Round: %d    %s",
                     game->roundNumber,
                     phaseText);
        }

        if (dealerCardHidden || dealerHidesTotal(game->run.dealerId)) {
            if (game->dealer.handCount == 1) {
                snprintf(dealerTotal, sizeof(dealerTotal), "Total: ??");
            } else {
                snprintf(dealerTotal,
                         sizeof(dealerTotal),
                         "Total: ?? / ??");
            }
        } else if (game->dealer.handCount == 1) {
            dealerValue = calculateDealerHandValue(
                         game,
                         &game->dealer.hands[0],
                         game->run.dealerId);
            snprintf(dealerTotal,
                     sizeof(dealerTotal),
                     "Total: %d",
                     dealerValue);
        } else {
            snprintf(dealerTotal,
                     sizeof(dealerTotal),
                     "Total: %d / %d",
                     calculateDealerHandValue(
                         game,
                         &game->dealer.hands[0],
                         game->run.dealerId),
                     calculateDealerHandValue(
                         game,
                         &game->dealer.hands[1],
                         game->run.dealerId));
        }

        displayHandValue =
            calculatePlayerHandValueForIndex(game, displayHandIndex);

        snprintf(playerTotal,
                 sizeof(playerTotal),
                 "Total: %d",
                 displayHandValue);

        if (game->mode == MODE_CLASSIC) {
            if (game->player.handCount == 1) {
                snprintf(statusText,
                         sizeof(statusText),
                         "Dealer: %s    Chips: %d    Bet: %d",
                         dealerInfo->name,
                         game->player.chips,
                         game->player.bets[displayHandIndex]);
            } else {
                snprintf(statusText,
                         sizeof(statusText),
                         "Dealer: %s    Chips: %d    Bet: %d (hand %d/%d)",
                         dealerInfo->name,
                         game->player.chips,
                         game->player.bets[displayHandIndex],
                         displayHandIndex + 1,
                         game->player.handCount);
            }
        } else if (game->player.handCount == 1) {
            snprintf(statusText,
                     sizeof(statusText),
                     "Floor: %d    Dealer: %s    Marks: %d    Chips: %d    Bet: %d",
                     game->run.floor,
                     dealerInfo->name,
                     game->run.marks,
                     game->player.chips,
                     game->player.bets[displayHandIndex]);
        } else {
            snprintf(statusText,
                     sizeof(statusText),
                     "Floor: %d    Dealer: %s    Marks: %d    Chips: %d    Bet: %d (hand %d/%d)",
                     game->run.floor,
                     dealerInfo->name,
                     game->run.marks,
                     game->player.chips,
                     game->player.bets[displayHandIndex],
                     displayHandIndex + 1,
                     game->player.handCount);
        }

        effectiveStatusWidth = terminalWidth - tablePadding;

        if ((int)strlen(statusText) > effectiveStatusWidth) {
            snprintf(dealerName,
                     sizeof(dealerName),
                     "%s",
                     dealerInfo->name);
            if (game->mode == MODE_CLASSIC) {
                snprintf(statusText,
                         sizeof(statusText),
                         "Dealer: ");
                prefixLength = (int)strlen(statusText);

                if (game->player.handCount == 1) {
                    snprintf(statusText,
                             sizeof(statusText),
                             "    Chips: %d    Bet: %d",
                             game->player.chips,
                             game->player.bets[displayHandIndex]);
                } else {
                    snprintf(statusText,
                             sizeof(statusText),
                             "    Chips: %d    Bet: %d (hand %d/%d)",
                             game->player.chips,
                             game->player.bets[displayHandIndex],
                             displayHandIndex + 1,
                             game->player.handCount);
                }
            } else {
                snprintf(statusText,
                         sizeof(statusText),
                         "Floor: %d    Dealer: ",
                         game->run.floor);
                prefixLength = (int)strlen(statusText);

                if (game->player.handCount == 1) {
                    snprintf(statusText,
                             sizeof(statusText),
                             "    Marks: %d    Chips: %d    Bet: %d",
                             game->run.marks,
                             game->player.chips,
                             game->player.bets[displayHandIndex]);
                } else {
                    snprintf(statusText,
                             sizeof(statusText),
                             "    Marks: %d    Chips: %d    Bet: %d (hand %d/%d)",
                             game->run.marks,
                             game->player.chips,
                             game->player.bets[displayHandIndex],
                             displayHandIndex + 1,
                             game->player.handCount);
                }
            }

            suffixLength = (int)strlen(statusText);
            availableNameWidth = effectiveStatusWidth - prefixLength - suffixLength;

            if (availableNameWidth >= 2 &&
                (int)strlen(dealerName) > availableNameWidth) {
                dealerName[availableNameWidth - 2] = '.';
                dealerName[availableNameWidth - 1] = '.';
                dealerName[availableNameWidth] = '\0';
            }

            if (availableNameWidth < 2) {
                dealerName[0] = '.';
                dealerName[1] = '.';
                dealerName[2] = '\0';
            }

            if (game->mode == MODE_CLASSIC) {
                if (game->player.handCount == 1) {
                    snprintf(statusText,
                             sizeof(statusText),
                             "Dealer: %s    Chips: %d    Bet: %d",
                             dealerName,
                             game->player.chips,
                             game->player.bets[displayHandIndex]);
                } else {
                    snprintf(statusText,
                             sizeof(statusText),
                             "Dealer: %s    Chips: %d    Bet: %d (hand %d/%d)",
                             dealerName,
                             game->player.chips,
                             game->player.bets[displayHandIndex],
                             displayHandIndex + 1,
                             game->player.handCount);
                }
            } else if (game->player.handCount == 1) {
                snprintf(statusText,
                         sizeof(statusText),
                         "Floor: %d    Dealer: %s    Marks: %d    Chips: %d    Bet: %d",
                         game->run.floor,
                         dealerName,
                         game->run.marks,
                         game->player.chips,
                         game->player.bets[displayHandIndex]);
            } else {
                snprintf(statusText,
                         sizeof(statusText),
                         "Floor: %d    Dealer: %s    Marks: %d    Chips: %d    Bet: %d (hand %d/%d)",
                         game->run.floor,
                         dealerName,
                         game->run.marks,
                         game->player.chips,
                         game->player.bets[displayHandIndex],
                         displayHandIndex + 1,
                         game->player.handCount);
            }

            if ((int)strlen(statusText) > effectiveStatusWidth) {
                if (effectiveStatusWidth >= 3) {
                    statusText[effectiveStatusWidth - 3] = '.';
                    statusText[effectiveStatusWidth - 2] = '.';
                    statusText[effectiveStatusWidth - 1] = '.';
                    statusText[effectiveStatusWidth] = '\0';
                } else {
                    statusText[0] = '\0';
                }
            }
        }

        printf("%s%s%s", scrReset(), SCR_CLEAR, SCR_HOME);

        const char *modeText =
            game->mode == MODE_CLASSIC ? "[CLASSIC]" : "[ROGUELIKE]";

        char headerText[32];

        snprintf(headerText, sizeof(headerText), "TTY21  %s", modeText);

        printf(SCR_CURSOR, TITLE_ROW,
               (terminalWidth - (int)strlen(headerText)) / 2 + 1);
        printf("%sTTY21%s  %s%s%s",
               scrBold(), scrReset(),
               scrDim(), modeText, scrReset());

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
        drawDealerHands(game, tablePadding, dealerCardHidden);

        printHandHeader("PLAYER",
                        playerTotal,
                        PLAYER_LABEL_ROW,
                        tablePadding,
                        tableWidth);
        drawPlayerHands(game, tablePadding);

        // Status and input help use full terminal width; dealer context stays table-aligned.
        moveToTableRow(ROW_STATUS, tablePadding);
        printf("%s", statusText);
        printf(SCR_ERASE_EOL);

        if (game->state == STATE_BET) {
            hintText = "Type a bet and press Enter.";
        } else if (game->state == STATE_PLAYER_TURN) {
            hintText =
                "Keys: h hit   s stand   d double   p split   k peek   q quit";
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

        if (game->run.extraTwistCount == 0) {
            snprintf(twistsLine,
                     sizeof(twistsLine),
                     "Twist: %s — %s",
                     twistInfo->name,
                     twistInfo->description);
        } else {
            len = snprintf(twistsLine,
                           sizeof(twistsLine),
                           "Twists: %s",
                           twistInfo->name);

            for (i = 0;
                 i < game->run.extraTwistCount && len < (int)sizeof(twistsLine);
                 i++) {
                const TwistInfo *extraInfo;

                extraInfo = getTwistInfo(game->run.extraTwists[i]);
                len += snprintf(twistsLine + len,
                                sizeof(twistsLine) - (size_t)len,
                                " | %s",
                                extraInfo->name);
            }
        }

        printf("%s%s%s",
               scrDim(),
               twistsLine,
               scrReset());

        moveToTableRow(ROW_ORACLE, tablePadding);
        printf(SCR_ERASE_EOL);

        oracleText[0] = '\0';

        if (dealerShowsNextCard(game->run.dealerId) &&
            game->state == STATE_PLAYER_TURN &&
            game->deck.top < DECK_SIZE) {
            snprintf(oracleText,
                     sizeof(oracleText),
                     "Next card: %s%s",
                     CARD_RANKS[game->deck.cards[game->deck.top].rank],
                     CARD_SUITS[game->deck.cards[game->deck.top].suit]);
        }

        if (game->player.perkOwned[PERK_MARKED_CARD] &&
            game->player.perkCharges[PERK_MARKED_CARD] > 0 &&
            game->deck.top < DECK_SIZE) {
            int nextIndex;
            int nextCount;

            nextCount = DECK_SIZE - game->deck.top;

            if (nextCount > 3) {
                nextCount = 3;
            }

            len = (int)strlen(oracleText);

            if (len > 0) {
                len += snprintf(oracleText + len,
                                sizeof(oracleText) - (size_t)len,
                                " | Next:");
            } else {
                len += snprintf(oracleText + len,
                                sizeof(oracleText) - (size_t)len,
                                "Next:");
            }

            for (nextIndex = 0; nextIndex < nextCount; nextIndex++) {
                const Card *nextCard;

                nextCard = &game->deck.cards[game->deck.top + nextIndex];
                len += snprintf(oracleText + len,
                                sizeof(oracleText) - (size_t)len,
                                " %s%s",
                                CARD_RANKS[nextCard->rank],
                                CARD_SUITS[nextCard->suit]);
            }
        }

        if (oracleText[0] != '\0') {
            printf("%s%s%s", scrDim(), oracleText, scrReset());
        }

        renderLog(game, tablePadding, terminalWidth);

        fflush(stdout);
    } else if (!isatty(STDOUT_FILENO)) {
        renderGameStateFallback(game, hideDealerCard);
    }
}

// Prints the fallback game state for non-full-screen output.
void renderGameStateFallback(const Game *game, int hideDealerCard)
{
    const DealerInfo *dealerInfo;
    const TwistInfo *twistInfo;
    int dealerValues[2];
    int dealerCardHidden;
    int displayHandIndex;
    int playerValue;
    int cardIndex;
    int dealerHandIndex;

    dealerInfo = getDealerInfo(game->run.dealerId);
    twistInfo = getTwistInfo(game->run.twistId);

    for (dealerHandIndex = 0;
         dealerHandIndex < game->dealer.handCount;
         dealerHandIndex++) {
        dealerValues[dealerHandIndex] =
            calculateDealerHandValue(
                         game,
                         &game->dealer.hands[dealerHandIndex],
                         game->run.dealerId);
    }
    dealerCardHidden = hideDealerCard &&
                       !game->peekActive &&
                       !twistActive(game, TWIST_SNIPER);

    displayHandIndex = game->player.activeHand;

    if (displayHandIndex >= game->player.handCount) {
        displayHandIndex = game->player.handCount - 1;
    }

    if (displayHandIndex < 0) {
        displayHandIndex = 0;
    }

    playerValue =
        calculatePlayerHandValueForIndex(game, displayHandIndex);

    printf("Round: %d\n", game->roundNumber);
    printf("Dealer (%s): ", dealerInfo->name);

    for (dealerHandIndex = 0;
         dealerHandIndex < game->dealer.handCount;
         dealerHandIndex++) {
        if (dealerHandIndex > 0) {
            printf(" | ");
        }

        for (cardIndex = 0;
             cardIndex < game->dealer.hands[dealerHandIndex].count;
             cardIndex++) {
            if (dealerCardHidden && cardIndex == 0) {
                printf("??");
            } else {
                printCard(&game->dealer.hands[dealerHandIndex].cards[cardIndex]);
            }

            if (cardIndex < game->dealer.hands[dealerHandIndex].count - 1) {
                printf(" ");
            }
        }
    }

    if (dealerCardHidden || dealerHidesTotal(game->run.dealerId)) {
        if (game->dealer.handCount == 1) {
            printf(" | Total value: ??\n");
        } else {
            printf(" | Total value: ?? / ??\n");
        }
    } else {
        if (game->dealer.handCount == 1) {
            printf(" | Total value: %d\n", dealerValues[0]);
        } else {
            printf(" | Total value: %d / %d\n",
                   dealerValues[0],
                   dealerValues[1]);
        }
    }

    if (dealerInfo->rule1 != NULL) {
        printf("Rule: %s\n", dealerInfo->rule1);
    }

    if (twistInfo->id != TWIST_NONE) {
        printf("Twist: %s — %s\n",
               twistInfo->name,
               twistInfo->description);
    }

    if (game->player.handCount > 1) {
        printf("Player (hand %d/%d): ",
               displayHandIndex + 1,
               game->player.handCount);
    } else {
        printf("Player: ");
    }

    for (cardIndex = 0;
         cardIndex < game->player.hands[displayHandIndex].count;
         cardIndex++) {
        if (cardIndex > 0) {
            printf(" ");
        }

        printCard(&game->player.hands[displayHandIndex].cards[cardIndex]);
    }

    printf(" | Total value: %d\n", playerValue);
    printf("Chips: %d | Bet: %d\n",
           game->player.chips,
           game->player.bets[displayHandIndex]);
 
    {
        int logIndex;
        int sourceIndex;

        for (logIndex = 0; logIndex < LOG_SIZE; logIndex++) {
            sourceIndex = logIndex - (LOG_SIZE - game->log.count);

            if (sourceIndex >= 0) {
                printf("%s\n", game->log.lines[sourceIndex]);
            }
        }
    }
}

// Prints a fixed menu block with disabled entries dimmed.
void renderMenu(const char *title,
                const MenuItem *items,
                int itemCount,
                int disabledMask,
                int selectedIndex,
                int startRow) {
    int terminalWidth;
    int terminalHeight;
    int menuAreaWidth;
    int menuAreaPadding;
    int titlePadding;
    int itemIndex;
    int isDisabled;
    int rowWidth;
    int rowPadding;

    initColors();
    getTerminalSize(&terminalWidth, &terminalHeight);

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
        menuAreaWidth = terminalWidth - tablePadding * 2;
        menuAreaPadding = tablePadding;

        if (menuAreaWidth < 1 ||
            menuAreaWidth < (int)strlen(title)) {
            menuAreaWidth = terminalWidth;
            menuAreaPadding = 0;
        }

        titlePadding = menuAreaPadding +
                       (menuAreaWidth - (int)strlen(title)) / 2;

        if (titlePadding < 0) {
            titlePadding = 0;
        }

        printf(SCR_CURSOR, startRow - 1, titlePadding + 1);
        printf(SCR_ERASE_EOL);
        printf("%s%s%s", scrBold(), title, scrReset());

        for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
            isDisabled = (disabledMask & (1 << itemIndex)) != 0;
            rowWidth = 2 + 3 + 2 +
                       (int)strlen(items[itemIndex].label);

            menuAreaWidth = terminalWidth - tablePadding * 2;
            menuAreaPadding = tablePadding;

            if (menuAreaWidth < rowWidth) {
                menuAreaWidth = terminalWidth;
                menuAreaPadding = 0;
            }

            rowPadding = menuAreaPadding +
                         (menuAreaWidth - rowWidth) / 2;

            if (rowPadding < 0) {
                rowPadding = 0;
            }

            printf(SCR_CURSOR, startRow + itemIndex, rowPadding + 1);
            printf(SCR_ERASE_EOL);

            if (isDisabled) {
                printf("%s  [%c]  %s%s",
                       scrDim(),
                       items[itemIndex].hotkey,
                       items[itemIndex].label,
                       scrReset());
            } else {
                if (itemIndex == selectedIndex) {
                    printf("%s> ", scrBold());
                } else {
                    printf("  ");
                }

                printf("%s[%c]%s  ",
                       scrDim(),
                       items[itemIndex].hotkey,
                       scrReset());

                if (itemIndex == selectedIndex) {
                    printf("%s%s%s",
                           scrBold(),
                           items[itemIndex].label,
                           scrReset());
                } else {
                    printf("%s", items[itemIndex].label);
                }
            }
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

// Renders the between-floor perk shop.
void renderShop(const Game *game,
                const MenuItem *items,
                int selectedIndex) {
    int terminalWidth;
    int terminalHeight;
    int tableWidth;
    int shopPadding;
    int titlePadding;
    int itemRow;
    int perkIndex;
    int isOwned;
    int canAfford;
    int rowWidth;
    int menuAreaWidth;
    int menuAreaPadding;
    int rowPadding;

    initColors();
    getTerminalSize(&terminalWidth, &terminalHeight);

    if (isatty(STDOUT_FILENO) &&
        !renderTerminalSizeOk(&terminalWidth,
                              &terminalHeight)) {
        renderTooSmallScreen(terminalWidth, terminalHeight);
        return;
    }

    tableWidth = getDynamicTableWidth(game);

    if (isatty(STDOUT_FILENO) &&
        terminalWidth >= MIN_TERMINAL_WIDTH &&
        terminalHeight >= MIN_TERMINAL_HEIGHT) {
        renderHideCursor();
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
        printf("Marks: %d", game->run.marks);

        moveToTableRow(ROW_SHOP_CHIPS, shopPadding);
        printf("Chips: %d", game->player.chips);

        for (perkIndex = 0; perkIndex < PERK_COUNT; perkIndex++) {
            itemRow = SHOP_ITEM_START_ROW + perkIndex;
            isOwned = game->player.perkOwned[perkIndex];
            canAfford = game->run.marks >= perkCost((PerkId)perkIndex);

            rowWidth = 2 + 3 + 2 +
                       (int)strlen(items[perkIndex].label);
            menuAreaWidth = tableWidth;
            menuAreaPadding = shopPadding;

            if (menuAreaWidth < rowWidth) {
                menuAreaWidth = terminalWidth;
                menuAreaPadding = 0;
            }

            rowPadding = menuAreaPadding +
                         (menuAreaWidth - rowWidth) / 2;

            if (rowPadding < 0) {
                rowPadding = 0;
            }

            printf(SCR_CURSOR, itemRow, rowPadding + 1);
            printf(SCR_ERASE_EOL);

            if (isOwned || !canAfford) {
                printf("%s", scrDim());
            } else if (perkIndex == selectedIndex) {
                printf("%s", scrBold());
            }

            if (perkIndex == selectedIndex) {
                printf("> ");
            } else {
                printf("  ");
            }

            printf("%s[%c]%s  ",
                   scrDim(),
                   items[perkIndex].hotkey,
                   scrReset());

            if (isOwned || !canAfford) {
                printf("%s%s%s",
                       scrDim(),
                       items[perkIndex].label,
                       scrReset());
            } else if (perkIndex == selectedIndex) {
                printf("%s%s%s",
                       scrBold(),
                       items[perkIndex].label,
                       scrReset());
            } else {
                printf("%s", items[perkIndex].label);
            }
        }

        moveToTableRow(SHOP_DESCRIPTION_ROW, shopPadding);
        printf(SCR_ERASE_EOL);
        printf("%s%s%s",
               scrDim(),
               perkDescription((PerkId)selectedIndex),
               scrReset());

        moveToTableRow(SHOP_HINT_ROW, shopPadding);
        printf(SCR_ERASE_EOL);
        printf("%sKeys: 1-9/A-G buy   Q close%s",
               scrDim(),
               scrReset());
        renderLog(game, shopPadding, terminalWidth);
        fflush(stdout);
    } else {
        printf("SHOP\n");
        printf("Marks: %d | Chips: %d\n\n",
               game->run.marks,
               game->player.chips);

        for (perkIndex = 0; perkIndex < PERK_COUNT; perkIndex++) {
            isOwned = game->player.perkOwned[perkIndex];
            canAfford = game->run.marks >= perkCost((PerkId)perkIndex);

            if (perkIndex == selectedIndex) {
                printf("> ");
            } else {
                printf("  ");
            }

            if (isOwned || !canAfford) {
                printf("%s[%c]  %s%s\n",
                       scrDim(),
                       items[perkIndex].hotkey,
                       items[perkIndex].label,
                       scrReset());
            } else {
                printf("[%c]  %s\n",
                       items[perkIndex].hotkey,
                       items[perkIndex].label);
            }

            printf("%s%s%s\n",
                   scrDim(),
                   perkDescription((PerkId)perkIndex),
                   scrReset());
        }

        printf("%sKeys: 1-9/A-G buy   Q close%s\n",
               scrDim(),
               scrReset());
        fflush(stdout);
    }
}

// Prints a message on a fixed row after erasing the rest of that line.
void renderMessage(const char *msg, int row, MessageStyle style) {
    const char *prefix;
    const char *suffix;
    const char *displayMessage;
    int terminalWidth;
    int terminalHeight;
    int messagePadding;
    int maxWidth;
    int cut;
    char truncated[512];

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
    } else if (style == MSG_ACTION) {
        prefix = scrCyan();
        suffix = scrReset();
    } else if (style == MSG_DEALER) {
        prefix = scrMagenta();
        suffix = scrReset();
    }

    getTerminalSize(&terminalWidth, &terminalHeight);

    if (isatty(STDOUT_FILENO) &&
        !renderTerminalSizeOk(&terminalWidth,
                              &terminalHeight)) {
        renderTooSmallScreen(terminalWidth, terminalHeight);
        return;
    }

    messagePadding = tablePadding;

    if (isatty(STDOUT_FILENO) &&
        terminalWidth >= MIN_TERMINAL_WIDTH &&
        terminalHeight >= MIN_TERMINAL_HEIGHT) {
        displayMessage = msg;
        maxWidth = terminalWidth - messagePadding - 2;

        if (maxWidth < 8) {
            maxWidth = 8;
        }

        if ((int)strlen(msg) > maxWidth) {
            cut = maxWidth - 3;

            if (cut < 1) {
                cut = 1;
            }

            if (cut > (int)sizeof(truncated) - 4) {
                cut = (int)sizeof(truncated) - 4;
            }

            memcpy(truncated, msg, (size_t)cut);
            truncated[cut] = '.';
            truncated[cut + 1] = '.';
            truncated[cut + 2] = '.';
            truncated[cut + 3] = '\0';
            displayMessage = truncated;
        }

        printf(SCR_CURSOR, row, messagePadding + 1);
        printf(SCR_ERASE_EOL);
        printf("%s%s%s", prefix, displayMessage, suffix);
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
