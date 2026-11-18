#define _DEFAULT_SOURCE
#include "render_help.h"
#include "render_control.h"
#include "render_rows.h"
#include "render_terminal.h"
#include "terminal.h"
#include "render_style.h"
#include "render_layout.h"
#include "round_rules.h"
#include "round_economy.h"
#include "dealers.h"
#include "twists.h"
#include "perks.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
    terminalGetSize(&terminalWidth, &terminalHeight);
    renderInitColors();

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
            printf("Floor: %d | Marks: %d | Chips: %d | Bet: %d",
                   game->run.floor, game->run.marks,
                   game->player.chips,
                   game->player.bets[game->player.activeHand]);
        } else {
            printf("Chips: %d | Bet: %d", game->player.chips,
                   game->player.bets[game->player.activeHand]);
        }
        bottomRow++;

        printf(SCR_CURSOR, bottomRow, leftColumn);
        printf("Min bet: %d    Max bet: %d",
               getMinBet(game, game->run.floor, game->mode),
               getMaxBet(game, getMinBet(game, game->run.floor, game->mode)));
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
            printf("\nFloor: %d | Marks: %d | Chips: %d | Bet: %d\n",
                   game->run.floor, game->run.marks,
                   game->player.chips,
                   game->player.bets[game->player.activeHand]);
        } else {
            printf("\nChips: %d | Bet: %d\n", game->player.chips,
                   game->player.bets[game->player.activeHand]);
        }
        printf("Min bet: %d    Max bet: %d\n",
               getMinBet(game, game->run.floor, game->mode),
               effectiveMaxBet(game));
        printf("\nPress any key to return.\n");
        fflush(stdout);
    }
}
