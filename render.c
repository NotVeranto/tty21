#include "render.h"

#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define TABLE_MIN_WIDTH 72
#define TABLE_MIN_HEIGHT 35
#define CARD_WIDTH 11
#define CARD_GAP 1
#define CARDS_PER_ROW 6
#define CARD_CONTENT_WIDTH 9
#define CARD_HEIGHT 7
#define TABLE_WIDTH (CARDS_PER_ROW * CARD_WIDTH + (CARDS_PER_ROW - 1) * CARD_GAP)

#define TITLE_ROW 1
#define SEPARATOR_ROW 2
#define ROUND_ROW 4
#define DEALER_LABEL_ROW 6
#define DEALER_CARD_ROW 8
#define PLAYER_LABEL_ROW 16
#define PLAYER_CARD_ROW 18
#define STATUS_ROW 26
#define HINT_ROW 27

static const char *CARD_RANKS[] = {
    "", "A", "2", "3", "4", "5", "6",
    "7", "8", "9", "10", "J", "Q", "K"
};

static const char *CARD_SUITS[] = {
    "♥", "♦", "♣", "♠"
};

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

// Checks whether the fixed table grid fits the current terminal.
static int canRenderTable(const Game *game, int *width, int *height) {
    getTerminalSize(width, height);

    return isatty(STDOUT_FILENO) &&
           *width >= TABLE_MIN_WIDTH &&
           *height >= TABLE_MIN_HEIGHT &&
           game->dealer.hand.count <= CARDS_PER_ROW &&
           game->player.hand.count <= CARDS_PER_ROW;
}

// Moves the cursor to a table row and left-aligns content with the table.
static void moveToTableRow(int row, int tablePadding) {
    printf(SCR_CURSOR, row, tablePadding + 1);
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
                            int tablePadding) {
    int remainingWidth;

    remainingWidth = TABLE_WIDTH -
                     (int)strlen(label) -
                     (int)strlen(totalText);

    if (remainingWidth < 1) {
        remainingWidth = 1;
    }

    moveToTableRow(row, tablePadding);
    printf(SCR_BOLD "%s" SCR_RESET "%*s%s",
           label,
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
            printf(SCR_RED);
        }

        printf("%s", CARD_SUITS[card->suit]);
        printf(SCR_RESET);
        printf("    |");
    } else {
        printf("|       %2s|", rankName);
    }
}

// Draws one complete row of cards.
static void drawCards(const Hand *hand,
                      int startRow,
                      int tablePadding,
                      int hideFirstCard) {
    int lineIndex;
    int cardIndex;

    for (lineIndex = 0; lineIndex < CARD_HEIGHT; lineIndex++) {
        moveToTableRow(startRow + lineIndex, tablePadding);

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
    int terminalWidth;
    int terminalHeight;
    int tablePadding;
    int separatorIndex;
    char roundText[64];
    char dealerTotal[32];
    char playerTotal[32];
    char statusText[64];

    if (canRenderTable(game, &terminalWidth, &terminalHeight)) {
        tablePadding = (terminalWidth - TABLE_WIDTH) / 2;

        snprintf(roundText,
                 sizeof(roundText),
                 "Round: %d",
                 game->roundNumber);

        if (hideDealerCard && game->dealer.hand.count > 0) {
            snprintf(dealerTotal, sizeof(dealerTotal), "Total: ??");
        } else {
            snprintf(dealerTotal,
                     sizeof(dealerTotal),
                     "Total: %d",
                     game->dealer.hand.value);
        }

        snprintf(playerTotal,
                 sizeof(playerTotal),
                 "Total: %d",
                 game->player.hand.value);

        snprintf(statusText,
                 sizeof(statusText),
                 "Chips: %d    Bet: %d",
                 game->player.chips,
                 game->player.bet);

        printf(SCR_RESET SCR_CLEAR SCR_HOME);

        printf(SCR_CURSOR,
               TITLE_ROW,
               (terminalWidth - (int)strlen("TTY21")) / 2 + 1);
        printf(SCR_BOLD "TTY21" SCR_RESET);

        moveToTableRow(SEPARATOR_ROW, tablePadding);

        for (separatorIndex = 0;
             separatorIndex < TABLE_WIDTH;
             separatorIndex++) {
            printf("-");
        }

        printCenteredAtRow(roundText, ROUND_ROW, terminalWidth);
        printHandHeader("DEALER",
                        dealerTotal,
                        DEALER_LABEL_ROW,
                        tablePadding);
        drawCards(&game->dealer.hand,
                  DEALER_CARD_ROW,
                  tablePadding,
                  hideDealerCard);

        printHandHeader("PLAYER",
                        playerTotal,
                        PLAYER_LABEL_ROW,
                        tablePadding);
        drawCards(&game->player.hand,
                  PLAYER_CARD_ROW,
                  tablePadding,
                  0);

        moveToTableRow(STATUS_ROW, tablePadding);
        printf("%s", statusText);

        moveToTableRow(HINT_ROW, tablePadding);
        printf(SCR_DIM "Keys: H hit   S stand" SCR_RESET);
        printf(SCR_RESET);

        fflush(stdout);
    } else {
        printGameState(game, hideDealerCard);
    }
}

// Prints a message on a fixed row after erasing the rest of that line.
void renderMessage(const char *msg, int row) {
    int terminalWidth;
    int terminalHeight;

    getTerminalSize(&terminalWidth, &terminalHeight);

    if (isatty(STDOUT_FILENO) &&
        terminalWidth >= TABLE_MIN_WIDTH &&
        terminalHeight >= TABLE_MIN_HEIGHT) {
        printf(SCR_CURSOR, row, 1);
        printf(SCR_ERASE_EOL);
        printf("%s", msg);
        printf(SCR_RESET);
        fflush(stdout);
    } else {
        printf("%s\n", msg);
        fflush(stdout);
    }
}
