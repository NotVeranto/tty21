#define _DEFAULT_SOURCE
#include "render.h"
#include "render_style.h"
#include "render_layout.h"
#include "render_log.h"
#include "render_cards.h"
#include "render_table_info.h"
#include "terminal.h"

#include "round_cards.h"
#include "round_rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "round_economy.h"

#define TABLE_MIN_HEIGHT MIN_TERMINAL_HEIGHT

#define TITLE_ROW 1
#define SEPARATOR_ROW 2
#define ROUND_ROW 3
#define DEALER_LABEL_ROW 4
#define DEALER_CARD_ROW 5
#define PLAYER_LABEL_ROW 13
#define PLAYER_CARD_ROW 14

static int tablePadding;
static int colorEnabled = 1;
static int cursorHidden;

// Pauses visible round presentation without delaying fast or non-interactive runs.
void renderPause(const Game *game, int ms) {
    if (game == NULL || game->fastMode || ms <= 0 ||
        !isatty(STDOUT_FILENO)) {
        return;
    }

    usleep((unsigned int)ms * 1000U);
}


// Returns the 1-based column that centers content within the terminal.


// Prints a consistent centered title, optional subtitle, and dim separator.


// Draws the shared dim box border used by framed screens.




int renderTerminalDimensionsOk(int width, int height) {
    return terminalDimensionsOk(width, height);
}

int renderTerminalSizeOk(int *width, int *height) {
    terminalGetSize(width, height);
    return renderTerminalDimensionsOk(*width, *height);
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
        // Split sized on '\n' and center each line.
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
    if (strncmp(label, "DEALER", 6) == 0) {
        printf("%s%s%s%s%*s%s%s%s%s",
               scrMagenta(), scrBold(), label, scrReset(),
               remainingWidth, "", scrMagenta(), scrBold(),
               totalText, scrReset());
    } else {
        printf("%s%s%s%s%*s%s%s%s%s",
               scrGreen(), scrBold(), label, scrReset(),
               remainingWidth, "", scrGreen(), scrBold(),
               totalText, scrReset());
    }
}



void renderResetScreen(void) {
    renderInitColors();
    if (!isatty(STDOUT_FILENO)) {
        return;
    }
    printf("%s%s", SCR_CLEAR, SCR_HOME);
    renderShowCursor();
    tablePadding = 0;
    renderMessagesSetPadding(0);
    cursorHidden = 0;
    fflush(stdout);
}



// Clears and redraws the complete table on fixed rows.




void renderTable(const Game *game, int hideDealerCard) {
    const char *phaseText;
    int terminalWidth;
    int terminalHeight;
    int tableWidth;
    int dealerCardHidden;
    int displayHandIndex;
    int displayHandValue;
    int dealerValue;
    int bossActive;
    char roundText[64];
    char dealerTotal[32];
    char playerTotal[32];

    renderInitColors();

    bossActive = dealerIsBoss(game->run.dealerId);
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

    terminalGetSize(&terminalWidth, &terminalHeight);

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
        renderMessagesSetPadding(tablePadding);

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
        renderDrawDealerHands(game, tablePadding, dealerCardHidden);

        printHandHeader("PLAYER",
                        playerTotal,
                        PLAYER_LABEL_ROW,
                        tablePadding,
                        tableWidth);
        renderDrawPlayerHands(game, tablePadding);

        renderStatusLine(game, terminalWidth, tablePadding);

        renderTableInfo(game, terminalWidth, tablePadding);
        renderEventLog(game, tablePadding, terminalWidth);

        fflush(stdout);
    } else if (isatty(STDOUT_FILENO)) {
        printf("%s%s%s", scrReset(), SCR_CLEAR, SCR_HOME);
        renderGameStateFallback(game, hideDealerCard);
        fflush(stdout);
    } else {
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

    renderStatusLine(game, terminalWidth, tablePadding);
    renderEventLog(game, tablePadding, terminalWidth);
}

// Renders the roguelike shop with a list of available purchases and services.


// Prints a message on a fixed row after erasing the rest of that line.











