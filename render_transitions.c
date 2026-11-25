#define _DEFAULT_SOURCE
#include "render_transitions.h"
#include "render_control.h"
#include "render_terminal.h"
#include "terminal.h"
#include "render_style.h"
#include "render_layout.h"
#include "dealers.h"
#include "round_economy.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void printFloorTransitionLine(int row,
                                     int column,
                                     const char *text,
                                     int centered) {
    char content[35];
    int textLength;
    int offset;

    textLength = (int)strlen(text);
    if (textLength > 34) {
        textLength = 34;
    }

    offset = centered ? (34 - textLength) / 2 : 1;
    if (offset + textLength > 34) {
        textLength = 34 - offset;
    }

    memset(content, ' ', 34);
    content[34] = '\0';
    memcpy(content + offset, text, (size_t)textLength);

    printf(SCR_CURSOR, row, column);
    printf("%s|%s%s%s|%s",
           scrMuted(), scrReset(), content, scrMuted(), scrReset());
}

void renderFloorTransition(const Game *game) {
    const DealerInfo *dealerInfo;
    char floorText[32];
    char dealerText[64];
    char minBetText[32];
    int terminalWidth;
    int terminalHeight;
    int boxWidth;
    int boxLeft;
    int boxTop;

    if (game == NULL || game->fastMode || !isatty(STDOUT_FILENO)) {
        return;
    }

    renderInitColors();
    if (!renderTerminalSizeOk(&terminalWidth, &terminalHeight)) {
        return;
    }

    boxWidth = 36;
    boxLeft = getCenteredColumnForWidth(terminalWidth, boxWidth);
    boxTop = (terminalHeight - 7) / 2 + 1;
    dealerInfo = getDealerInfo(game->run.dealerId);

    snprintf(floorText, sizeof(floorText), "FLOOR %02d", game->run.floor);
    snprintf(dealerText, sizeof(dealerText), "Dealer: %.25s", dealerInfo->name);
    snprintf(minBetText, sizeof(minBetText), "Min Bet: %d",
             getMinBet(game, game->run.floor, game->mode));

    printf("%s%s", scrReset(), SCR_CLEAR);
    printf(SCR_CURSOR, boxTop, boxLeft);
    printf("%s+----------------------------------+%s", scrMuted(), scrReset());
    printFloorTransitionLine(boxTop + 1, boxLeft, floorText, 1);
    printFloorTransitionLine(boxTop + 2, boxLeft, "THE STAKES RISE", 1);
    printFloorTransitionLine(boxTop + 3, boxLeft, "", 1);
    printFloorTransitionLine(boxTop + 4, boxLeft, dealerText, 0);
    printFloorTransitionLine(boxTop + 5, boxLeft, minBetText, 0);
    printf(SCR_CURSOR, boxTop + 6, boxLeft);
    printf("%s+----------------------------------+%s", scrMuted(), scrReset());
    fflush(stdout);
}

static void printBossPrepBoxLine(int row,
                                 int column,
                                 const char *text) {
    printf(SCR_CURSOR, row, column);
    printf("%s|%s  %-58.58s  %s|%s",
           scrMuted(), scrReset(), text, scrMuted(), scrReset());
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

    renderInitColors();
    terminalGetSize(&terminalWidth, &terminalHeight);
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
