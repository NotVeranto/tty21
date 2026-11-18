#define _DEFAULT_SOURCE
#include "render_status.h"
#include "render_control.h"
#include "render_rows.h"
#include "render_style.h"
#include "dealers.h"
#include "round_cards.h"

#include <stdio.h>
#include <string.h>

static void moveToTableRow(int row, int padding) {
    printf(SCR_CURSOR, row, padding + 1);
}

static void printStyledStatusText(const char *statusText) {
    const char *cursor = statusText;
    int first = 1;

    while (*cursor != '\0') {
        const char *separator = strstr(cursor, " | ");
        size_t length = separator != NULL
                            ? (size_t)(separator - cursor) : strlen(cursor);
        char segment[256];
        const char *style = "";

        if (length >= sizeof(segment)) length = sizeof(segment) - 1;
        memcpy(segment, cursor, length);
        segment[length] = '\0';

        if (strncmp(segment, "Floor:", 6) == 0) {
            char *openBracket = strchr(segment, '[');
            char *closeBracket = strchr(segment, ']');
            if (openBracket != NULL && closeBracket != NULL && closeBracket > openBracket) {
                *openBracket = '\0';
                *closeBracket = '\0';
                if (!first) printf(" %s|%s ", scrDim(), scrReset());
                printf("%s%s%s%s[%s]%s", scrBold(), segment, scrReset(), scrDim(), openBracket + 1, scrReset());
                first = 0;
                if (separator == NULL) break;
                cursor = separator + 3;
                continue;
            }
            style = scrBold();
        } else if (strncmp(segment, "Marks:", 6) == 0) style = scrCyan();
        else if (strncmp(segment, "Chips:", 6) == 0) style = scrYellow();

        if (!first) printf(" %s|%s ", scrDim(), scrReset());
        printf("%s%s%s", style, segment, scrReset());
        first = 0;
        if (separator == NULL) break;
        cursor = separator + 3;
    }
}

static void renderStatusLineAtChips(const Game *game,
                                    int terminalWidth,
                                    int padding,
                                    int displayedChips) {
    const DealerInfo *dealerInfo;
    int prefixLength;
    int suffixLength;
    int availableNameWidth;
    int effectiveStatusWidth;
    int displayHandIndex;
    int cardsLeft;
    char dealerName[64];
    char statusText[256];
    char loanText[64];
    char progressEquals[11];
    char progressDots[11];
    int progress;

    cardsLeft = DECK_SIZE - game->deck.top;
    progress = game->run.floor % 10;
    if (progress < 0) progress = 0;
    memset(progressEquals, '=', (size_t)progress);
    progressEquals[progress] = '\0';
    memset(progressDots, '.', (size_t)(10 - progress));
    progressDots[10 - progress] = '\0';
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
    snprintf(dealerName, sizeof(dealerName), "%s", dealerInfo->name);
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
                         "Chips: %d | Bet: %d | Deck: %d cards",
                         displayedChips,
                         game->player.bets[displayHandIndex],
                         cardsLeft);
            } else {
                snprintf(statusText,
                         sizeof(statusText),
                         "Chips: %d | Bet: %d (hand %d/%d) | Deck: %d cards",
                         displayedChips,
                         game->player.bets[displayHandIndex],
                         displayHandIndex + 1,
                         game->player.handCount,
                         cardsLeft);
            }
        } else if (game->player.handCount == 1) {
            snprintf(statusText,
                     sizeof(statusText),
                     "Floor: %d [%s%s] | %s | Marks: %d | Chips: %d | Bet: %d | Deck: %d cards%s",
                     game->run.floor, progressEquals, progressDots,
                     dealerName,
                     game->run.marks,
                     displayedChips,
                     game->player.bets[displayHandIndex],
                     cardsLeft,
                     loanText);
        } else {
            snprintf(statusText,
                     sizeof(statusText),
                     "Floor: %d [%s%s] | %s | Marks: %d | Chips: %d | Bet: %d (hand %d/%d) | Deck: %d cards%s",
                     game->run.floor, progressEquals, progressDots,
                     dealerName,
                     game->run.marks,
                     displayedChips,
                     game->player.bets[displayHandIndex],
                     displayHandIndex + 1,
                     game->player.handCount,
                     cardsLeft,
                     loanText);
        }

        effectiveStatusWidth = terminalWidth - padding;

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
                snprintf(statusText, sizeof(statusText),
                         "Floor: %d [%s%s] | ",
                         game->run.floor, progressEquals, progressDots);
                prefixLength = (int)strlen(statusText);

                if (game->player.handCount == 1) {
                    snprintf(statusText,
                             sizeof(statusText),
                             " | Marks: %d | Chips: %d | Bet: %d | Deck: %d cards%s",
                             game->run.marks,
                             displayedChips,
                             game->player.bets[displayHandIndex],
                             cardsLeft,
                             loanText);
                } else {
                    snprintf(statusText,
                             sizeof(statusText),
                             " | Marks: %d | Chips: %d | Bet: %d (hand %d/%d) | Deck: %d cards%s",
                             game->run.marks,
                             displayedChips,
                             game->player.bets[displayHandIndex],
                             displayHandIndex + 1,
                             game->player.handCount,
                             cardsLeft,
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
                             "Floor: %d | %s | Marks: %d | Chips: %d | Bet: %d | Deck: %d",
                             game->run.floor,
                             dealerName,
                             game->run.marks,
                             displayedChips,
                             game->player.bets[displayHandIndex],
                             cardsLeft);
                } else {
                    snprintf(statusText,
                             sizeof(statusText),
                             "Floor: %d | %s | Marks: %d | Chips: %d | Bet: %d (hand %d/%d) | Deck: %d",
                             game->run.floor,
                             dealerName,
                             game->run.marks,
                             displayedChips,
                             game->player.bets[displayHandIndex],
                             displayHandIndex + 1,
                             game->player.handCount,
                             cardsLeft);
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
    printStyledStatusText(statusText);
    printf(SCR_ERASE_EOL);
}

void renderStatusLine(const Game *game, int terminalWidth, int padding) {
    renderStatusLineAtChips(game, terminalWidth, padding,
                            game->player.chips);
}

