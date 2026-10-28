#include "render.h"

#include "round_cards.h"
#include "round_rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "round_economy.h"

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
#define SHOP_ITEM_START_ROW 12
#define SHOP_DESCRIPTION_ROW 31
#define SHOP_HINT_ROW 32

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
const char *scrReset(void) {
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
const char *scrBold(void) {
    return colorEnabled ? "\033[1m" : "";
}

// Returns the ANSI dim sequence when colors are enabled.
const char *scrDim(void) {
    return colorEnabled ? "\033[2m" : "";
}

// Returns the color associated with a cursed card marker.
static const char *curseColor(CurseId id) {
    switch (id) {
        case CURSED_BLEED: return scrRed();
        case CURSED_FROST: return scrCyan();
        case CURSED_ECHO:  return scrYellow();
        default: return "";
    }
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

// Returns the 1-based column that centers content within the terminal.
static int getCenteredColumn(int contentWidth) {
    int terminalWidth;
    int terminalHeight;
    int column;

    getTerminalSize(&terminalWidth, &terminalHeight);
    (void)terminalHeight;

    column = (terminalWidth - contentWidth) / 2 + 1;
    if (column < 1) {
        column = 1;
    }

    return column;
}


/* Prints a consistent centered title, optional subtitle, and dim separator. */
void renderScreenHeader(const char *title,
                        const char *subtitle,
                        int row) {
    int terminalWidth;
    int terminalHeight;
    int separatorWidth;
    int titleColumn;

    getTerminalSize(&terminalWidth, &terminalHeight);
    (void)terminalHeight;

    titleColumn = getCenteredColumn((int)strlen(title));
    printf(SCR_CURSOR, row, titleColumn);
    printf("%s%s%s", scrBold(), title, scrReset());

    if (subtitle != NULL) {
        printf(SCR_CURSOR, row + 1, getCenteredColumn((int)strlen(subtitle)));
        printf("%s%s%s", scrDim(), subtitle, scrReset());
    }

    separatorWidth = terminalWidth < 60 ? terminalWidth : 60;
    if (separatorWidth < 1) {
        separatorWidth = 1;
    }

    printf(SCR_CURSOR,
           row + (subtitle != NULL ? 2 : 1),
           getCenteredColumn(separatorWidth));
    printf("%s", scrDim());
    while (separatorWidth-- > 0) {
        putchar('-');
    }
    printf("%s", scrReset());
}

/* Draws the shared dim box border used by framed screens. */
static void printBoxBorder(int row, int column, int width) {
    int index;

    printf(SCR_CURSOR, row, column);
    printf("%s+", scrDim());
    for (index = 0; index < width - 2; index++) {
        putchar('-');
    }
    printf("+%s", scrReset());
}

static void printBoxSides(int row, int column, int width) {
    printf(SCR_CURSOR, row, column);
    printf("%s|%s", scrDim(), scrReset());
    printf(SCR_CURSOR, row, column + width - 1);
    printf("%s|%s", scrDim(), scrReset());
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
        if (card->cursed != CURSED_NONE) {
            printf("|%-2s      %s*%s|",
                   rankName,
                   curseColor((CurseId)card->cursed),
                   scrReset());
        } else {
            printf("|%-2s       |", rankName);
        }
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

    if (game->dealer.handCount <= 0) {
        return;
    }

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

    if (handValue > 21) {
        style = scrRed();
    } else {
        style = handIndex == game->player.activeHand &&
                game->player.activeHand < game->player.handCount
                    ? scrBold()
                    : scrDim();
    }

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

static void printBossPrepBoxLine(int row,
                                 int column,
                                 const char *text) {
    printf(SCR_CURSOR, row, column);
    printf("%s|%s  %-58.58s  %s|%s",
           scrDim(), scrReset(), text, scrDim(), scrReset());
}

void renderBossPrep(const Game *game) {
    enum { BOX_WIDTH = 64, BOX_HEIGHT = 12 };
    const DealerInfo *dealerInfo;
    const char *rule2;
    const char *power;
    int terminalWidth;
    int terminalHeight;
    int boxColumn;
    int boxRow;
    int i;
    char title[256];
    char line[256];

    dealerInfo = getDealerInfo(game->run.dealerId);
    rule2 = dealerInfo->rule2;
    power = dealerPowerRule2(game->run.dealerId, game->run.floor);
    if (rule2 == NULL && power != NULL) {
        rule2 = power;
    }

    initColors();
    getTerminalSize(&terminalWidth, &terminalHeight);
    (void)terminalWidth;
    boxColumn = getCenteredColumn(BOX_WIDTH);
    boxRow = (terminalHeight - BOX_HEIGHT) / 2 + 1;
    if (boxRow < 1) {
        boxRow = 1;
    }

    renderHideCursor();
    printf("%s%s%s", scrReset(), SCR_CLEAR, SCR_HOME);

    snprintf(title, sizeof(title), "TTY21 - BOSS: %.44s",
             dealerInfo->name != NULL ? dealerInfo->name : "Unknown");
    printBoxBorder(boxRow, boxColumn, BOX_WIDTH);
    renderScreenHeader(title, "Review the rules before entering",
                       boxRow + 1);
    for (i = boxRow + 1; i < boxRow + BOX_HEIGHT - 1; i++) {
        printBoxSides(i, boxColumn, BOX_WIDTH);
    }

    snprintf(line, sizeof(line), "Rule 1: %.240s",
             dealerInfo->rule1 != NULL ? dealerInfo->rule1 : "None");
    printBossPrepBoxLine(boxRow + 4, boxColumn, line);

    snprintf(line, sizeof(line), "Rule 2: %.240s",
             rule2 != NULL ? rule2 : "None");
    printBossPrepBoxLine(boxRow + 5, boxColumn, line);

    printBossPrepBoxLine(boxRow + 6, boxColumn, "");

    snprintf(line, sizeof(line), "Disable Rule 1 for %d marks? (y/n)",
             game->bossRulePaid);
    printBossPrepBoxLine(boxRow + 7, boxColumn, line);

    snprintf(line, sizeof(line), "Current marks: %d", game->run.marks);
    printBossPrepBoxLine(boxRow + 8, boxColumn, line);

    if (game->run.marks < game->bossRulePaid) {
        printBossPrepBoxLine(boxRow + 9, boxColumn, "Not enough marks.");
    } else {
        printBossPrepBoxLine(boxRow + 9, boxColumn, "");
    }

    printBoxBorder(boxRow + BOX_HEIGHT - 1, boxColumn, BOX_WIDTH);
    fflush(stdout);
}

void renderResetScreen(void) {
    initColors();
    if (!isatty(STDOUT_FILENO)) {
        return;
    }
    printf("%s%s", SCR_CLEAR, SCR_HOME);
    renderShowCursor();
    tablePadding = 0;
    cursorHidden = 0;
    fflush(stdout);
}

// Clears and redraws the complete table on fixed rows.
static void drawStatusLine(const Game *game,
                              int terminalWidth,
                              int padding) {
    const DealerInfo *dealerInfo;
    int prefixLength;
    int suffixLength;
    int availableNameWidth;
    int effectiveStatusWidth;
    int displayHandIndex;
    char dealerName[64];
    char statusText[256];
    char loanText[64];

    loanText[0] = '\0';

    if (game->mode == MODE_ROGUELIKE &&
        game->loanAmount > 0) {
        snprintf(loanText,
                 sizeof(loanText),
                 "  Loan: %d by F%d",
                 game->loanAmount,
                 game->loanDueFloor);
    }

    dealerInfo = getDealerInfo(game->run.dealerId);
    if (game->bossRuleDisabled && dealerIsBoss(game->run.dealerId)) {
        snprintf(dealerName, sizeof(dealerName), "%s [Rule 1 Disabled]", dealerInfo->name);
    } else {
        snprintf(dealerName, sizeof(dealerName), "%s", dealerInfo->name);
    }
    displayHandIndex = game->player.activeHand;
    if (displayHandIndex >= game->player.handCount) {
        displayHandIndex = game->player.handCount - 1;
    }
    if (displayHandIndex < 0) {
        displayHandIndex = 0;
    }

        if (game->mode == MODE_CLASSIC) {
            if (game->player.handCount == 1) {
                snprintf(statusText,
                         sizeof(statusText),
                         "Chips: %d    Bet: %d",
                         game->player.chips,
                         game->player.bets[displayHandIndex]);
            } else {
                snprintf(statusText,
                         sizeof(statusText),
                         "Chips: %d    Bet: %d (hand %d/%d)",
                         game->player.chips,
                         game->player.bets[displayHandIndex],
                         displayHandIndex + 1,
                         game->player.handCount);
            }
        } else if (game->player.handCount == 1) {
            snprintf(statusText,
                     sizeof(statusText),
                     "Floor: %d    Dealer: %s    Marks: %d    Chips: %d    Bet: %d%s",
                     game->run.floor,
                     dealerName,
                     game->run.marks,
                     game->player.chips,
                     game->player.bets[displayHandIndex],
                     loanText);
        } else {
            snprintf(statusText,
                     sizeof(statusText),
                     "Floor: %d    Dealer: %s    Marks: %d    Chips: %d    Bet: %d (hand %d/%d)%s",
                     game->run.floor,
                     dealerName,
                     game->run.marks,
                     game->player.chips,
                     game->player.bets[displayHandIndex],
                     displayHandIndex + 1,
                     game->player.handCount,
                     loanText);
        }

        effectiveStatusWidth = terminalWidth - tablePadding;

        if ((int)strlen(statusText) > effectiveStatusWidth) {
            if (game->mode == MODE_CLASSIC) {
                if (effectiveStatusWidth >= 3) {
                    statusText[effectiveStatusWidth - 3] = '.';
                    statusText[effectiveStatusWidth - 2] = '.';
                    statusText[effectiveStatusWidth - 1] = '.';
                    statusText[effectiveStatusWidth] = '\0';
                } else {
                    statusText[0] = '\0';
                }
            } else {
                if (game->bossRuleDisabled && dealerIsBoss(game->run.dealerId)) {
                    snprintf(dealerName, sizeof(dealerName), "%s [Rule 1 Disabled]", dealerInfo->name);
                } else {
                    snprintf(dealerName, sizeof(dealerName), "%s", dealerInfo->name);
                }
                snprintf(statusText,
                         sizeof(statusText),
                         "Floor: %d    Dealer: ",
                         game->run.floor);
                prefixLength = (int)strlen(statusText);

                if (game->player.handCount == 1) {
                    snprintf(statusText,
                             sizeof(statusText),
                             "    Marks: %d    Chips: %d    Bet: %d%s",
                             game->run.marks,
                             game->player.chips,
                             game->player.bets[displayHandIndex],
                             loanText);
                } else {
                    snprintf(statusText,
                             sizeof(statusText),
                             "    Marks: %d    Chips: %d    Bet: %d (hand %d/%d)%s",
                             game->run.marks,
                             game->player.chips,
                             game->player.bets[displayHandIndex],
                             displayHandIndex + 1,
                             game->player.handCount,
                             loanText);
                }

                suffixLength = (int)strlen(statusText);
                availableNameWidth = effectiveStatusWidth -
                                     prefixLength - suffixLength;

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

                if (game->player.handCount == 1) {
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
        }

    moveToTableRow(ROW_STATUS, padding);
    printf("%s", statusText);
    printf(SCR_ERASE_EOL);
}

// Returns the strongest category among active twists for banner coloring.
static const char *twistBannerColor(const Game *game) {
    TwistCategory worst = TWIST_CAT_FAVORABLE;
    int i;

    if (game->run.twistId != TWIST_NONE &&
        !game->run.twistBlocked) {
        TwistCategory cat =
            getTwistInfo(game->run.twistId)->category;
        if (cat == TWIST_CAT_HOSTILE) return scrRed();
        if (cat == TWIST_CAT_NEUTRAL) worst = TWIST_CAT_NEUTRAL;
    }

    for (i = 0; i < game->run.extraTwistCount; i++) {
        TwistCategory cat =
            getTwistInfo(game->run.extraTwists[i])->category;
        if (cat == TWIST_CAT_HOSTILE) return scrRed();
        if (cat == TWIST_CAT_NEUTRAL) worst = TWIST_CAT_NEUTRAL;
    }

    if (worst == TWIST_CAT_NEUTRAL) return scrDim();
    return scrGreen();
}

// Appends an Oracle item, separated from existing items by " | ".
static void appendOracle(char *buf, size_t size, const char *text) {
    size_t len = strlen(buf);

    if (len == 0) {
        snprintf(buf, size, "%s", text);
    } else if (len + 3 < size) {
        snprintf(buf + len, size - len, " | %s", text);
    }
}

// Reports whether dealt cards are depleted of low ranks.
static int isDeckRunningHot(const Game *game) {
    int lowCount;
    int highCount;
    int i;

    if (game->deck.top < 20) {
        return 0;
    }

    lowCount = 0;
    highCount = 0;

    for (i = 0; i < game->deck.top; i++) {
        Rank r = game->deck.cards[i].rank;
        if (r >= TWO && r <= SIX) {
            lowCount++;
        } else if (r >= TEN || r == ACE) {
            highCount++;
        }
    }

    /* Hot when lows outnumber highs by more than 1.5x.
       Avoid floating point: lowCount * 2 > highCount * 3. */
    return lowCount * 2 > highCount * 3;
}

void renderTable(const Game *game, int hideDealerCard) {
    const DealerInfo *dealerInfo;
    const TwistInfo *twistInfo;
    const char *hintText;
    const char *phaseText;
    int terminalWidth;
    int terminalHeight;
    int tableWidth;
    int dealerCardHidden;
    int displayHandIndex;
    int displayHandValue;
    int dealerValue;
    int bossActive;
    int i;
    int len;
    char rulesText[256];
    char twistsLine[256];
    char oracleText[256];
    char roundText[64];
    char dealerTotal[32];
    char playerTotal[32];

    initColors();

    dealerInfo = getDealerInfo(game->run.dealerId);
    bossActive = dealerIsBoss(game->run.dealerId);
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

        printf("%s%s%s", scrReset(), SCR_CLEAR, SCR_HOME);

        const char *modeText =
            game->mode == MODE_CLASSIC ? "[CLASSIC]" : "[ROGUELIKE]";

        renderScreenHeader("TTY21", modeText, TITLE_ROW);
        if (bossActive) {
            int bossColumn;

            bossColumn = getCenteredColumn((int)strlen(modeText)) +
                         (int)strlen(modeText) + 2;
            printf(SCR_CURSOR, TITLE_ROW + 1, bossColumn);
            printf("%s%sBOSS%s", scrRed(), scrBold(), scrReset());
        }

        printCenteredAtRow(roundText, ROUND_ROW, terminalWidth);
        printHandHeader(bossActive ? "DEALER  BOSS" : "DEALER",
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

        drawStatusLine(game, terminalWidth, tablePadding);

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

        {
            const char *rule1 = dealerInfo->rule1;
            const char *rule2 = dealerInfo->rule2;
            const char *power = dealerPowerRule2(game->run.dealerId, game->run.floor);
            if (rule2 == NULL && power != NULL) rule2 = power;
            if (rule1 != NULL || rule2 != NULL) {
            if (rule2 == NULL) {
                snprintf(rulesText,
                         sizeof(rulesText),
                         "Rule: %s",
                         rule1);
            } else if (rule1 == NULL) {
                snprintf(rulesText, sizeof(rulesText), "Rule: %s", rule2);
            } else {
                snprintf(rulesText, sizeof(rulesText), "Rules: %s | %s", rule1, rule2);
            }
            printf("%s%s%s", scrDim(), rulesText, scrReset());
            }
        }

        moveToTableRow(ROW_TWIST, tablePadding);
        printf(SCR_ERASE_EOL);

        if (game->mode == MODE_ROGUELIKE) {
            int showPrimary;
            int showExtras;
            int showBlockedPrimary;
            int hasTwistName;

            showPrimary = game->run.twistId != TWIST_NONE &&
                          !game->run.twistBlocked;
            showBlockedPrimary = game->run.twistId != TWIST_NONE &&
                                 game->run.twistBlocked;
            showExtras = 0;

            for (i = 0; i < game->run.extraTwistCount; i++) {
                if (game->run.extraTwists[i] != TWIST_NONE) {
                    showExtras = 1;
                    break;
                }
            }

            twistsLine[0] = '\0';

            if (!showPrimary &&
                !showBlockedPrimary &&
                !showExtras) {
                if (game->safetyNetArmed) {
                    printf("%sSAFETY NET%s",
                           scrDim(),
                           scrReset());
                } else {
                    printf(SCR_ERASE_EOL);
                }
            } else if ((showPrimary || showBlockedPrimary) &&
                       !showExtras) {
                snprintf(twistsLine,
                         sizeof(twistsLine),
                         "Twist: %s%s - %s",
                         twistInfo->name,
                         showBlockedPrimary ? " (blocked)" : "",
                         twistInfo->description);
                printf("%s%s%s%s",
                       twistBannerColor(game),
                       game->safetyNetArmed ? "SAFETY NET  |  " : "",
                       twistsLine,
                       scrReset());
            } else {
                len = snprintf(twistsLine,
                               sizeof(twistsLine),
                               "Twists: ");
                hasTwistName = 0;

                if (showPrimary || showBlockedPrimary) {
                    len += snprintf(twistsLine + len,
                                    sizeof(twistsLine) - (size_t)len,
                                    "%s%s",
                                    twistInfo->name,
                                    showBlockedPrimary ? " (blocked)" : "");
                    hasTwistName = 1;
                }

                for (i = 0;
                     i < game->run.extraTwistCount &&
                     len < (int)sizeof(twistsLine);
                     i++) {
                    const TwistInfo *extraInfo;

                    if (game->run.extraTwists[i] == TWIST_NONE) {
                        continue;
                    }

                    extraInfo = getTwistInfo(game->run.extraTwists[i]);

                    if (hasTwistName) {
                        len += snprintf(twistsLine + len,
                                        sizeof(twistsLine) - (size_t)len,
                                        " | ");
                    }

                    len += snprintf(twistsLine + len,
                                    sizeof(twistsLine) - (size_t)len,
                                    "%s",
                                    extraInfo->name);
                    hasTwistName = 1;
                }

                if (hasTwistName) {
                    printf("%s%s%s%s",
                           twistBannerColor(game),
                           game->safetyNetArmed ? "SAFETY NET  |  " : "",
                           twistsLine,
                           scrReset());
                } else if (game->safetyNetArmed) {
                    printf("%sSAFETY NET%s",
                           scrDim(),
                           scrReset());
                } else {
                    printf(SCR_ERASE_EOL);
                }
            }
        } else {
            printf(SCR_ERASE_EOL);
        }

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

        if (twistActive(game, TWIST_NO_RESHUFFLE) &&
            game->deck.top <= DECK_SIZE) {
            char deckText[32];
            snprintf(deckText, sizeof(deckText),
                     "Deck: %d/%d",
                     DECK_SIZE - game->deck.top,
                     DECK_SIZE);
            appendOracle(oracleText, sizeof(oracleText), deckText);
        }

        if (game->mode == MODE_ROGUELIKE &&
            game->run.floor >= 10 &&
            isDeckRunningHot(game)) {
            appendOracle(oracleText, sizeof(oracleText),
                         "running hot");
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

void renderMenuLogRefresh(const Game *game) {
    int terminalWidth;
    int terminalHeight;

    if (!isatty(STDOUT_FILENO)) {
        return;
    }

    if (!renderTerminalSizeOk(&terminalWidth,
                              &terminalHeight)) {
        return;
    }

    drawStatusLine(game, terminalWidth, tablePadding);
    renderLog(game, tablePadding, terminalWidth);
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
        if (game->run.twistBlocked) {
            printf("Twist: %s (blocked) - %s\n",
                   twistInfo->name,
                   twistInfo->description);
        } else {
            printf("Twist: %s - %s\n",
                   twistInfo->name,
                   twistInfo->description);
        }
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
    printf("Chips: %d | Bet: %d",
           game->player.chips,
           game->player.bets[displayHandIndex]);

    if (game->mode == MODE_ROGUELIKE &&
        game->loanAmount > 0) {
        printf(" | Loan: %d by F%d",
               game->loanAmount,
               game->loanDueFloor);
    }

    printf("\n");

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
        titlePadding = getCenteredColumn((int)strlen(title)) - 1;

        printf(SCR_CURSOR, startRow - 1, titlePadding + 1);
        printf(SCR_ERASE_EOL);
        printf("%s%s%s", scrBold(), title, scrReset());

        for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
            isDisabled = (disabledMask & (1 << itemIndex)) != 0;
            rowWidth = 2 + 3 + 2 +
                       (int)strlen(items[itemIndex].label);

            rowPadding = getCenteredColumn(rowWidth) - 1;

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

// Renders the roguelike shop with a list of available purchases and services.
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
    int maxLabelWidth;
    int blockPadding;
    int blockWidth;

    initColors();
    getTerminalSize(&terminalWidth, &terminalHeight);
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

            printf(SCR_CURSOR, itemRow, blockPadding + 1);
            printf(SCR_ERASE_EOL);
            if (isDisabled) {
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
        renderLog(game, shopPadding, terminalWidth);
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

static void printHelpColumnLine(int row,
                               int column,
                               int width,
                               const char *text) {
    int length;

    length = (int)strlen(text);

    if (length > width) {
        length = width;
    }

    printf(SCR_CURSOR, row, column);
    printf(SCR_ERASE_EOL);
    printf("%.*s", length, text);
}

static int effectiveMaxBet(const Game *game) {
    int cap;
    int limit;

    cap = game->player.chips;
    if (game->mode == MODE_ROGUELIKE) {
        limit = game->player.chips;
        if (game->run.floor >= 12) {
            limit = game->player.chips / 3;
        } else if (game->run.floor >= 8) {
            limit = game->player.chips / 2;
        }
        if (limit > 0) {
            cap = limit;
        }
    }

    return cap;
}

static int shouldShowNextFloor(const Game *game) {
    return game->state == STATE_MENU &&
           game->mode == MODE_ROGUELIKE;
}

void renderHelpOverlay(const Game *game) {
    const DealerInfo *dealerInfo;
    const TwistInfo *twistInfo;
    const char *helpRule1;
    const char *helpRule2;
    const char *helpPower;
    int terminalWidth;
    int terminalHeight;
    int leftColumn;
    int columnWidth;
    int boxWidth;
    int boxLeft;
    int boxTop;
    int contentRow;
    int activeTwistCount;
    int ownedPerkCount;
    int index;
    int bottomRow;
    int showBox;
    char line[256];
    const char *helpTitle;
    const char *payoutBreakdownText;

    payoutBreakdownText = game->lastPayoutBreakdown;
    getTerminalSize(&terminalWidth, &terminalHeight);
    initColors();

    if (isatty(STDOUT_FILENO) &&
        terminalHeight >= 20) {
        dealerInfo = getDealerInfo(game->run.dealerId);
        helpRule1 = dealerInfo->rule1;
        helpRule2 = dealerInfo->rule2;
        helpPower = dealerPowerRule2(game->run.dealerId,
                                     game->run.floor);
        if (helpRule2 == NULL && helpPower != NULL) {
            helpRule2 = helpPower;
        }
        showBox = terminalWidth >= 70;
        helpTitle = "TTY21 - HELP";

        if (showBox) {
            boxWidth = terminalWidth - 8;

            if (boxWidth > 100) {
                boxWidth = 100;
            }

            if (boxWidth < 60) {
                boxWidth = 60;
            }

            boxLeft = getCenteredColumn(boxWidth);
            leftColumn = boxLeft + 3;
            columnWidth = boxWidth - 6;
            boxTop = 2;
            contentRow = boxTop + 4;
        } else {
            columnWidth = terminalWidth - 4;

            if (columnWidth < 20) {
                columnWidth = 20;
            }

            leftColumn = getCenteredColumn(columnWidth);

            boxTop = 1;
            contentRow = 4;
        }

        renderHideCursor();
        printf("%s%s", SCR_CLEAR, SCR_HOME);
        renderScreenHeader(helpTitle,
                           showBox ? "Current run" : NULL,
                           boxTop);

        printf(SCR_CURSOR, contentRow, leftColumn);
        printf("%sDealer:%s  %s%s",
               scrDim(),
               scrReset(),
               dealerIsBoss(game->run.dealerId) ? "BOSS " : "",
               dealerInfo->name);
        contentRow++;

        if (helpRule1 != NULL) {
            snprintf(line,
                     sizeof(line),
                     "  %s",
                     helpRule1);
            printHelpColumnLine(contentRow,
                                leftColumn,
                                columnWidth,
                                line);
            contentRow++;
        }

        if (helpRule2 != NULL) {
            snprintf(line,
                     sizeof(line),
                     "  %s",
                     helpRule2);
            printHelpColumnLine(contentRow,
                                leftColumn,
                                columnWidth,
                                line);
            contentRow++;
        }

        if (game->mode == MODE_ROGUELIKE) {
            activeTwistCount = 0;

            if (twistActive(game, game->run.twistId) &&
                game->run.twistId != TWIST_NONE) {
                activeTwistCount++;
            }

            activeTwistCount += game->run.extraTwistCount;

            printf(SCR_CURSOR, contentRow + 1, leftColumn);
            printf("%sTwists (%d active):%s",
                   scrDim(),
                   activeTwistCount,
                   scrReset());
            contentRow += 2;

            if (activeTwistCount == 0) {
                printHelpColumnLine(contentRow,
                                    leftColumn,
                                    columnWidth,
                                    "  None");
                contentRow++;
            } else {
                if (twistActive(game, game->run.twistId) &&
                    game->run.twistId != TWIST_NONE) {
                    twistInfo = getTwistInfo(game->run.twistId);
                    snprintf(line,
                             sizeof(line),
                             "  - %s: %s",
                             twistInfo->name,
                             twistInfo->description);
                    printHelpColumnLine(contentRow,
                                        leftColumn,
                                        columnWidth,
                                        line);
                    contentRow++;
                }

                for (index = 0;
                     index < game->run.extraTwistCount;
                     index++) {
                    twistInfo = getTwistInfo(game->run.extraTwists[index]);
                    snprintf(line,
                             sizeof(line),
                             "  - %s: %s",
                             twistInfo->name,
                             twistInfo->description);
                    printHelpColumnLine(contentRow,
                                        leftColumn,
                                        columnWidth,
                                        line);
                    contentRow++;
                }
            }
        }

        if (game->mode == MODE_ROGUELIKE) {
            printf(SCR_CURSOR, contentRow + 1, leftColumn);
            printf("%sPerks owned:%s",
                   scrDim(),
                   scrReset());
            contentRow += 2;

            ownedPerkCount = 0;

            for (index = 0; index < PERK_COUNT; index++) {
                if (game->player.perkOwned[index]) {
                    snprintf(line,
                             sizeof(line),
                             "  - %s: %s",
                             perkName((PerkId)index),
                             perkDescription((PerkId)index));
                    printHelpColumnLine(contentRow,
                                        leftColumn,
                                        columnWidth,
                                        line);
                    contentRow++;
                    ownedPerkCount++;
                }
            }

            if (ownedPerkCount == 0) {
                printHelpColumnLine(contentRow,
                                    leftColumn,
                                    columnWidth,
                                    "  None");
                contentRow++;
            }

            if (game->run.coldSnapActive) {
                printf(SCR_CURSOR, contentRow + 1, leftColumn);
                printf("%sCold snap pending%s",
                       scrDim(), scrReset());
                contentRow += 2;
            }

            if (game->run.perkLockedId >= 0) {
                printf(SCR_CURSOR, contentRow + 1, leftColumn);
                printf("%sLocked: %s (this floor)%s",
                       scrDim(),
                       perkName((PerkId)game->run.perkLockedId),
                       scrReset());
                contentRow += 2;
            }

            if (game->mode == MODE_ROGUELIKE &&
                game->lastAutosavedRound > 0) {
                char saveLine[64];
                printf(SCR_CURSOR, contentRow + 1, leftColumn);
                printf("%sSave:%s", scrDim(), scrReset());
                contentRow += 2;
                snprintf(saveLine, sizeof(saveLine),
                         "  Round %d", game->lastAutosavedRound);
                printHelpColumnLine(contentRow, leftColumn,
                                    columnWidth, saveLine);
                contentRow++;
            }

            }

        if (payoutBreakdownText[0] != '\0') {
            printf(SCR_CURSOR, contentRow + 1, leftColumn);
            printf("%sLast round:%s", scrDim(), scrReset());
            contentRow += 2;
            printHelpColumnLine(contentRow,
                                leftColumn,
                                columnWidth,
                                payoutBreakdownText);
            contentRow++;
        }

        if (shouldShowNextFloor(game)) {
            AdvanceForecast forecast;
            char nextLine[128];

            computeAdvanceForecast(game, &forecast);

            printf(SCR_CURSOR, contentRow + 1, leftColumn);
            printf("%sNext floor:%s", scrDim(), scrReset());
            contentRow += 2;

            snprintf(nextLine, sizeof(nextLine),
                     "  Fee %d, min bet %d",
                     forecast.fee, forecast.nextMinBet);
            printHelpColumnLine(contentRow, leftColumn,
                                columnWidth, nextLine);
            contentRow++;

            if (forecast.loanDue > 0) {
                snprintf(nextLine, sizeof(nextLine),
                         "  Loan due: %d chips", forecast.loanDue);
                printHelpColumnLine(contentRow, leftColumn,
                                    columnWidth, nextLine);
                contentRow++;
            }

            if (forecast.affordable) {
                snprintf(nextLine, sizeof(nextLine),
                         "  Post-advance: %d chips",
                         forecast.postChips);
            } else {
                snprintf(nextLine, sizeof(nextLine),
                         "  Blocked: need %d chips, have %d",
                         forecast.fee + forecast.nextMinBet +
                             forecast.loanDue,
                         game->player.chips);
            }
            printHelpColumnLine(contentRow, leftColumn,
                                columnWidth, nextLine);
            contentRow++;
        }

        bottomRow = contentRow + 1;

        printf(SCR_CURSOR, bottomRow, leftColumn);
        if (game->mode == MODE_ROGUELIKE) {
            printf("Floor: %d    Marks: %d    Chips: %d    Bet: %d",
                   game->run.floor, game->run.marks,
                   game->player.chips,
                   game->player.bets[game->player.activeHand]);
        } else {
            printf("Chips: %d    Bet: %d", game->player.chips,
                   game->player.bets[game->player.activeHand]);
        }
        bottomRow++;

        printf(SCR_CURSOR, bottomRow, leftColumn);
        printf("Min bet: %d    Max bet: %d",
               getMinBet(game, game->run.floor, game->mode),
               effectiveMaxBet(game));
        bottomRow += 2;

        printf(SCR_CURSOR, bottomRow, leftColumn);
        printf("%sPress any key to return.%s",
               scrDim(),
               scrReset());

        if (showBox) {
            int frameRow;

            printBoxBorder(boxTop - 1, boxLeft, boxWidth);
            for (frameRow = boxTop; frameRow <= bottomRow; frameRow++) {
                printBoxSides(frameRow, boxLeft, boxWidth);
            }
            printBoxBorder(bottomRow + 1, boxLeft, boxWidth);
        }
        fflush(stdout);
        return;
    }

    if (!isatty(STDOUT_FILENO)) {
        dealerInfo = getDealerInfo(game->run.dealerId);
        helpRule1 = dealerInfo->rule1;
        helpRule2 = dealerInfo->rule2;
        helpPower = dealerPowerRule2(game->run.dealerId,
                                     game->run.floor);
        if (helpRule2 == NULL && helpPower != NULL) {
            helpRule2 = helpPower;
        }
        printf("TTY21 - HELP\n");
        printf("============\n\n");
        printf("Dealer: %s%s\n",
               dealerIsBoss(game->run.dealerId) ? "BOSS " : "",
               dealerInfo->name);

        if (helpRule1 != NULL) {
            printf("  %s\n", helpRule1);
        }

        if (helpRule2 != NULL) {
            printf("  %s\n", helpRule2);
        }

        if (game->mode == MODE_ROGUELIKE) {
            activeTwistCount = 0;

            if (twistActive(game, game->run.twistId) &&
                game->run.twistId != TWIST_NONE) {
                activeTwistCount++;
            }

            activeTwistCount += game->run.extraTwistCount;

            printf("\nTwists (%d active):\n", activeTwistCount);

            if (activeTwistCount == 0) {
                printf("  None\n");
            } else {
                if (twistActive(game, game->run.twistId) &&
                    game->run.twistId != TWIST_NONE) {
                    twistInfo = getTwistInfo(game->run.twistId);
                    printf("  - %s: %s\n",
                           twistInfo->name,
                           twistInfo->description);
                }

                for (index = 0;
                     index < game->run.extraTwistCount;
                     index++) {
                    twistInfo = getTwistInfo(game->run.extraTwists[index]);
                    printf("  - %s: %s\n",
                           twistInfo->name,
                           twistInfo->description);
                }
            }
        }

        if (game->mode == MODE_ROGUELIKE) {
            printf("\nPerks owned:\n");
            ownedPerkCount = 0;

            for (index = 0; index < PERK_COUNT; index++) {
                if (game->player.perkOwned[index]) {
                    printf("  - %s: %s\n",
                           perkName((PerkId)index),
                           perkDescription((PerkId)index));
                    ownedPerkCount++;
                }
            }

            if (ownedPerkCount == 0) {
                printf("  None\n");
            }

            if (game->run.coldSnapActive) {
                printf("\nCold snap pending\n");
            }

            if (game->run.perkLockedId >= 0) {
                printf("\nLocked: %s (this floor)\n",
                       perkName((PerkId)game->run.perkLockedId));
            }

            if (game->mode == MODE_ROGUELIKE &&
                game->lastAutosavedRound > 0) {
                printf("\nSave:\n  Round %d\n",
                       game->lastAutosavedRound);
            }

            }

        if (payoutBreakdownText[0] != '\0') {
            printf("\nLast round:\n  %s\n",
                   payoutBreakdownText);
        }

        if (shouldShowNextFloor(game)) {
            AdvanceForecast forecast;

            computeAdvanceForecast(game, &forecast);
            printf("\nNext floor:\n");
            printf("  Fee %d, min bet %d\n",
                   forecast.fee, forecast.nextMinBet);
            if (forecast.loanDue > 0) {
                printf("  Loan due: %d chips\n", forecast.loanDue);
            }
            if (forecast.affordable) {
                printf("  Post-advance: %d chips\n",
                       forecast.postChips);
            } else {
                printf("  Blocked: need %d chips, have %d\n",
                       forecast.fee + forecast.nextMinBet +
                           forecast.loanDue,
                       game->player.chips);
            }
        }

        if (game->mode == MODE_ROGUELIKE) {
            printf("\nFloor: %d    Marks: %d    Chips: %d    Bet: %d\n",
                   game->run.floor, game->run.marks,
                   game->player.chips,
                   game->player.bets[game->player.activeHand]);
        } else {
            printf("\nChips: %d    Bet: %d\n", game->player.chips,
                   game->player.bets[game->player.activeHand]);
        }
        printf("Min bet: %d    Max bet: %d\n",
               getMinBet(game, game->run.floor, game->mode),
               effectiveMaxBet(game));
        printf("\nPress any key to return.\n");
        fflush(stdout);
    }
}

// Renders the horizontal player action bar.
void renderActionBar(const ActionId *actions,
                     int actionCount,
                     int selectedIndex,
                     int row,
                     int showHelp) {
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
    for (actionIndex = 0; actionIndex < actionCount; actionIndex++) {
        if (actionIndex > 0) {
            printf("   ");
        }

        if (actionIndex == selectedIndex) {
            printf("%s> %s[%c]%s %s%s%s",
                   scrBold(),
                   scrDim(),
                   actionToChar(actions[actionIndex]),
                   scrReset(),
                   scrBold(),
                   ACTION_NAMES[actions[actionIndex]],
                   scrReset());
        } else {
            printf("  %s[%c]%s %s",
                   scrDim(),
                   actionToChar(actions[actionIndex]),
                   scrReset(),
                   ACTION_NAMES[actions[actionIndex]]);
        }
    }

    if (showHelp) {
        printf("   %s[?]%s %shelp%s",
               scrDim(), scrReset(), scrDim(), scrReset());
    }
    fflush(stdout);
}
