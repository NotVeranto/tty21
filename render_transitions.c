#define _DEFAULT_SOURCE
#include "render_transitions.h"
#include "render_control.h"
#include "render_terminal.h"
#include "terminal.h"
#include "render_style.h"
#include "render_layout.h"
#include "render_canvas.h"
#include "render.h"
#include "dealers.h"
#include "round_economy.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define FLOOR_BOX_W 42
#define FLOOR_BOX_H 15
#define FLOOR_FRAME_MS 30

/* 5x5 block digits for the floor number. */
static const char *const DIGITS[10][5] = {
    { "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", "\xe2\x96\x88   \xe2\x96\x88", "\xe2\x96\x88   \xe2\x96\x88", "\xe2\x96\x88   \xe2\x96\x88", "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88" },
    { "  \xe2\x96\x88  ", " \xe2\x96\x88\xe2\x96\x88  ", "  \xe2\x96\x88  ", "  \xe2\x96\x88  ", " \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88 " },
    { "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", "    \xe2\x96\x88", "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", "\xe2\x96\x88    ", "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88" },
    { "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", "    \xe2\x96\x88", " \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", "    \xe2\x96\x88", "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88" },
    { "\xe2\x96\x88   \xe2\x96\x88", "\xe2\x96\x88   \xe2\x96\x88", "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", "    \xe2\x96\x88", "    \xe2\x96\x88" },
    { "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", "\xe2\x96\x88    ", "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", "    \xe2\x96\x88", "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88" },
    { "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", "\xe2\x96\x88    ", "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", "\xe2\x96\x88   \xe2\x96\x88", "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88" },
    { "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", "    \xe2\x96\x88", "   \xe2\x96\x88 ", "  \xe2\x96\x88  ", "  \xe2\x96\x88  " },
    { "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", "\xe2\x96\x88   \xe2\x96\x88", "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", "\xe2\x96\x88   \xe2\x96\x88", "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88" },
    { "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", "\xe2\x96\x88   \xe2\x96\x88", "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", "    \xe2\x96\x88", "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88" },
};

typedef struct {
    int boxH;         /* animated height of the card */
    int digitCols;    /* revealed columns of the big number */
    int taglineChars; /* typed characters of the tagline */
    int detail;       /* 0 hidden, 1 dim, 2 full */
    int labelOn;
} FloorFrame;

static void drawFloorFrame(const Game *game, const FloorFrame *frame,
                           int terminalWidth, int terminalHeight) {
    const DealerInfo *dealerInfo;
    const Glyphs *glyphs;
    char number[16];
    char text[96];
    int boxX;
    int boxTop;
    int fullTop;
    int innerX;
    int innerW;
    int digits;
    int digitX;
    int index;
    int row;
    int column;
    int dealerRow;

    glyphs = renderGlyphs();
    dealerInfo = getDealerInfo(game->run.dealerId);
    boxX = (terminalWidth - FLOOR_BOX_W) / 2 + 1;
    if (boxX < 1) {
        boxX = 1;
    }
    fullTop = (terminalHeight - FLOOR_BOX_H) / 2 + 1;
    if (fullTop < 1) {
        fullTop = 1;
    }
    boxTop = fullTop + (FLOOR_BOX_H - frame->boxH) / 2;

    canvasBlank();
    canvasBox(boxX, boxTop, FLOOR_BOX_W, frame->boxH, NULL, ST_BRASS,
              ST_FELT_TEXT, ST_GOLD_B);
    if (frame->boxH < FLOOR_BOX_H) {
        return;
    }
    innerX = boxX + 3;
    innerW = FLOOR_BOX_W - 6;

    if (frame->labelOn) {
        canvasPutCentered(boxX + 1, FLOOR_BOX_W - 2, fullTop + 2,
                          renderUtf8Enabled() ? "F  L  O  O  R"
                                              : "F L O O R",
                          ST_FELT_MUTED);
    }

    snprintf(number, sizeof(number), "%d", game->run.floor);
    digits = (int)strlen(number);
    if (renderUtf8Enabled()) {
        digitX = boxX + (FLOOR_BOX_W - (digits * 7 - 2)) / 2;
        for (row = 0; row < 5; row++) {
            for (index = 0; index < digits; index++) {
                column = index * 7;
                if (column < frame->digitCols) {
                    canvasPutTextN(digitX + column, fullTop + 3 + row,
                                   frame->digitCols - column,
                                   DIGITS[number[index] - '0'][row],
                                   ST_FELT_GOLD);
                }
            }
        }
    } else if (frame->digitCols > 0) {
        canvasPutCentered(boxX + 1, FLOOR_BOX_W - 2, fullTop + 5, number,
                          ST_FELT_GOLD);
    }

    canvasPutTextN(boxX + (FLOOR_BOX_W - 15) / 2, fullTop + 9,
                   frame->taglineChars, "THE STAKES RISE",
                   ST_FELT_PLAYER);
    if (frame->detail > 0) {
        RenderStyle label;
        RenderStyle value;

        label = frame->detail == 1 ? (RenderStyle)(ST_FELT_MUTED | RSTYLE_DIM)
                                   : ST_FELT_MUTED;
        value = frame->detail == 1 ? (RenderStyle)(ST_FELT_TEXT | RSTYLE_DIM)
                                   : ST_FELT_TEXT;
        canvasHLine(innerX, fullTop + 10, innerW, glyphs->rule,
                    ST_FELT_BRASS);
        dealerRow = fullTop + 11;
        canvasPutText(innerX, dealerRow, "Dealer", label);
        snprintf(text, sizeof(text), "%.25s",
                 dealerInfo != NULL && dealerInfo->name != NULL
                     ? dealerInfo->name : "Unknown");
        canvasPutFit(innerX + 9, dealerRow, innerW - 9, text, value);
        canvasPutText(innerX, fullTop + 12, "Min bet", label);
        snprintf(text, sizeof(text), "%d",
                 getMinBet(game, game->run.floor, game->mode));
        canvasPutText(innerX + 9, fullTop + 12, text, ST_FELT_GOLD);
    }
}

void renderFloorTransition(const Game *game) {
    static const int HEIGHTS[] = { 3, 5, 7, 9, 11, 13, 15 };
    FloorFrame frame;
    char number[16];
    int terminalWidth;
    int terminalHeight;
    int index;
    int step;
    int fullDigits;
    int animate;

    if (game == NULL || game->fastMode || !isatty(STDOUT_FILENO)) {
        return;
    }

    renderInitColors();
    if (!renderTerminalSizeOk(&terminalWidth, &terminalHeight)) {
        return;
    }
    if (!canvasBegin()) {
        return;
    }
    renderHideCursor();
    animate = renderMotionEnabled();

    snprintf(number, sizeof(number), "%d", game->run.floor);
    fullDigits = (int)strlen(number) * 7 - 2;

    canvasInvalidate();
    if (animate) {
        memset(&frame, 0, sizeof(frame));
        for (index = 0; index < 7; index++) {
            frame.boxH = HEIGHTS[index];
            drawFloorFrame(game, &frame, terminalWidth, terminalHeight);
            canvasFlush();
            renderPause(game, FLOOR_FRAME_MS);
        }
        frame.labelOn = 1;
        for (step = 1; step <= 4; step++) {
            frame.digitCols = fullDigits * step / 4;
            drawFloorFrame(game, &frame, terminalWidth, terminalHeight);
            canvasFlush();
            renderPause(game, FLOOR_FRAME_MS);
        }
        for (step = 1; step <= 15; step += 2) {
            frame.taglineChars = step > 15 ? 15 : step;
            drawFloorFrame(game, &frame, terminalWidth, terminalHeight);
            canvasFlush();
            renderPause(game, FLOOR_FRAME_MS);
        }
        frame.taglineChars = 15;
        frame.detail = 1;
        drawFloorFrame(game, &frame, terminalWidth, terminalHeight);
        canvasFlush();
        renderPause(game, 60);
    }
    memset(&frame, 0, sizeof(frame));
    frame.boxH = FLOOR_BOX_H;
    frame.labelOn = 1;
    frame.digitCols = fullDigits;
    frame.taglineChars = 15;
    frame.detail = 2;
    drawFloorFrame(game, &frame, terminalWidth, terminalHeight);
    canvasFlush();
}

/* Splits text into lines of at most `width` columns on word borders. */
static int wrapText(const char *text, int width, char lines[][96],
                    int maxLines) {
    int count;
    int length;
    const char *cursor;
    const char *space;
    int take;

    count = 0;
    cursor = text;
    if (width > 90) {
        width = 90;
    }
    while (*cursor != '\0' && count < maxLines) {
        while (*cursor == ' ') {
            cursor++;
        }
        length = (int)strlen(cursor);
        if (length <= width) {
            take = length;
        } else {
            take = width;
            space = cursor + width;
            while (space > cursor && *space != ' ') {
                space--;
            }
            if (space > cursor) {
                take = (int)(space - cursor);
            }
        }
        memcpy(lines[count], cursor, (size_t)take);
        lines[count][take] = '\0';
        count++;
        cursor += take;
    }
    return count;
}

static int drawRuleBlock(int x, int row, int width, const char *label,
                         const char *text) {
    char lines[2][96];
    int count;
    int index;

    canvasPutText(x, row, label, ST_GOLD);
    count = wrapText(text, width - 8, lines, 2);
    if (count == 0) {
        canvasPutText(x + 8, row, "None", ST_MUTED);
        return 1;
    }
    for (index = 0; index < count; index++) {
        canvasPutText(x + 8, row + index, lines[index], ST_PLAIN);
    }
    return count;
}

void renderBossPrep(const Game *game) {
    enum { BOX_WIDTH = 64, BOX_HEIGHT = 14 };
    const DealerInfo *dealerInfo;
    const char *rule2;
    const char *power;
    int terminalWidth;
    int terminalHeight;
    int boxWidth;
    int boxColumn;
    int boxRow;
    int x;
    int width;
    char title[256];
    char line[256];

    if (game == NULL) {
        return;
    }

    renderInitColors();
    if (isatty(STDOUT_FILENO) &&
        !renderTerminalSizeOk(&terminalWidth, &terminalHeight)) {
        renderTooSmallScreen(terminalWidth, terminalHeight);
        return;
    }

    dealerInfo = getDealerInfo(game->run.dealerId);
    rule2 = dealerInfo->rule2;
    power = dealerPowerRule2(game->run.dealerId, game->run.floor);
    if (rule2 == NULL && power != NULL) {
        rule2 = power;
    }

    if (!isatty(STDOUT_FILENO) || !canvasBegin()) {
        printf("TTY21 - BOSS: %s\n", dealerInfo->name != NULL
                                         ? dealerInfo->name : "Unknown");
        printf("Rule 1: %s\n", dealerInfo->rule1 != NULL
                                   ? dealerInfo->rule1 : "None");
        printf("Rule 2: %s\n", rule2 != NULL ? rule2 : "None");
        printf("Disable Rule 1 for %d marks? (y/n)\n", game->bossRulePaid);
        printf("Current marks: %d\n", game->run.marks);
        if (game->run.marks < game->bossRulePaid) {
            printf("Not enough marks.\n");
        }
        fflush(stdout);
        return;
    }

    terminalGetSize(&terminalWidth, &terminalHeight);
    boxWidth = BOX_WIDTH < terminalWidth - 2 ? BOX_WIDTH : terminalWidth - 2;
    boxColumn = (terminalWidth - boxWidth) / 2 + 1;
    boxRow = (terminalHeight - BOX_HEIGHT) / 2 + 1;
    if (boxRow < 1) {
        boxRow = 1;
    }

    renderHideCursor();
    snprintf(title, sizeof(title), "BOSS - %.44s",
             dealerInfo->name != NULL ? dealerInfo->name : "Unknown");

    canvasBlank();
    canvasBox(boxColumn, boxRow, boxWidth, BOX_HEIGHT, title, ST_BAD,
              ST_PLAIN, ST_BADGE_RED);
    x = boxColumn + 3;
    width = boxWidth - 6;

    canvasPutCentered(boxColumn, boxWidth, boxRow + 2,
                      "Review the rules before entering", ST_MUTED);
    canvasHLine(x, boxRow + 3, width, renderGlyphs()->rule, ST_BRASS);
    drawRuleBlock(x, boxRow + 4, width, "RULE 1",
                  dealerInfo->rule1 != NULL ? dealerInfo->rule1 : "");
    drawRuleBlock(x, boxRow + 6, width, "RULE 2", rule2 != NULL ? rule2 : "");

    snprintf(line, sizeof(line), "Disable Rule 1 for %d marks?",
             game->bossRulePaid);
    canvasPutText(x, boxRow + 9, line, ST_STRONG);
    {
        int cursor;

        cursor = x;
        cursor += canvasPutText(cursor, boxRow + 10, " Y ", ST_KEYCAP);
        cursor += canvasPutText(cursor, boxRow + 10, " Disable   ", ST_PLAIN);
        cursor += canvasPutText(cursor, boxRow + 10, " N ", ST_KEYCAP);
        canvasPutText(cursor, boxRow + 10, " Keep", ST_PLAIN);
    }
    snprintf(line, sizeof(line), "Current marks: %d", game->run.marks);
    canvasPutText(x, boxRow + 11, line, ST_ACCENT);
    if (game->run.marks < game->bossRulePaid) {
        canvasPutText(x + 24, boxRow + 11, "Not enough marks.", ST_BAD);
    }
    canvasInvalidate();
    canvasFlush();
}
