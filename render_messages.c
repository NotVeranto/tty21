#define _DEFAULT_SOURCE
#include "render_messages.h"
#include "render_canvas.h"
#include "render_control.h"
#include "render_rows.h"
#include "render_scene.h"
#include "render_terminal.h"
#include "render_style.h"
#include "render_layout.h"
#include "render.h"
#include "terminal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BANNER_REVEAL_FRAMES 6
#define BANNER_BLINKS 2

static RenderStyle messageStyle(MessageStyle style) {
    switch (style) {
        case MSG_WIN:    return ST_OK;
        case MSG_LOSS:   return ST_BAD;
        case MSG_PUSH:   return ST_WARN;
        case MSG_INFO:   return ST_MUTED;
        case MSG_ACTION: return ST_ACCENT;
        case MSG_DEALER: return ST_ROSE;
        default:         return ST_PLAIN;
    }
}

static RenderStyle badgeStyle(MessageStyle style) {
    switch (style) {
        case MSG_WIN:  return ST_BADGE_OK;
        case MSG_LOSS: return ST_BADGE_RED;
        case MSG_PUSH: return ST_BADGE_WARN;
        default:       return ST_BADGE_GOLD;
    }
}

/* Draws `text` but only the columns inside [clipL, clipR]. */
static void putClipped(int x, int row, const char *text, RenderStyle style,
                       int clipL, int clipR) {
    const unsigned char *p;
    char glyph[5];
    size_t length;
    int column;

    p = (const unsigned char *)text;
    column = x;
    while (*p != '\0') {
        length = 1;
        if (*p >= 0xF0) length = 4;
        else if (*p >= 0xE0) length = 3;
        else if (*p >= 0xC0) length = 2;
        memset(glyph, 0, sizeof(glyph));
        memcpy(glyph, p, length);
        if (column >= clipL && column <= clipR) {
            canvasPutGlyph(column, row, glyph, style);
        }
        p += length;
        column++;
    }
}

static void drawBanner(int x0, int width, const char *label,
                       RenderStyle badge, RenderStyle ornament, int radius) {
    const Glyphs *glyphs;
    char left[24];
    char right[24];
    char mid[160];
    int total;
    int start;
    int center;
    int clipL;
    int clipR;

    glyphs = renderGlyphs();
    snprintf(left, sizeof(left), "%s%s%s%s ", glyphs->fH, glyphs->fH,
             glyphs->fH, glyphs->capL);
    snprintf(right, sizeof(right), " %s%s%s%s", glyphs->capR, glyphs->fH,
             glyphs->fH, glyphs->fH);
    snprintf(mid, sizeof(mid), " %s ", label);

    total = canvasTextWidth(left) + canvasTextWidth(mid) +
            canvasTextWidth(right);
    start = x0 + (width - total) / 2;
    if (start < x0) {
        start = x0;
    }
    center = start + total / 2;
    clipL = center - radius;
    clipR = center + radius;

    putClipped(start, ROW_RESULT, left, ornament, clipL, clipR);
    putClipped(start + canvasTextWidth(left), ROW_RESULT, mid, badge,
               clipL, clipR);
    putClipped(start + canvasTextWidth(left) + canvasTextWidth(mid),
               ROW_RESULT, right, ornament, clipL, clipR);
}

void renderResultFlash(const Game *game,
                       const char *message,
                       MessageStyle style) {
    char fitted[160];
    int terminalWidth;
    int terminalHeight;
    int x0;
    int width;
    int frame;
    int total;
    int radius;

    if (game == NULL || message == NULL || game->fastMode ||
        !isatty(STDOUT_FILENO)) {
        return;
    }

    renderInitColors();
    if (!renderTerminalSizeOk(&terminalWidth, &terminalHeight)) {
        return;
    }
    if (!canvasBegin()) {
        return;
    }

    renderHudRegion(&x0, &width);
    if (width < 12) {
        x0 = 1;
        width = terminalWidth;
    }
    snprintf(fitted, sizeof(fitted), "%.*s", 60, message);
    if (canvasTextWidth(fitted) > width - 14) {
        /* never wider than the frame: cut and mark with dots */
        int keep;

        keep = width - 17;
        if (keep < 1) keep = 1;
        if (keep > (int)sizeof(fitted) - 4) {
            keep = (int)sizeof(fitted) - 4;
        }
        fitted[keep] = '\0';
        memcpy(fitted + keep, "...", sizeof("..."));
    }
    total = canvasTextWidth(fitted) + 16;

    canvasTouchRow(ROW_RESULT);
    for (frame = 1; frame <= BANNER_REVEAL_FRAMES; frame++) {
        canvasFill(x0, ROW_RESULT, width, 1, ST_PLAIN);
        radius = (total / 2 + 1) * frame / BANNER_REVEAL_FRAMES;
        drawBanner(x0, width, fitted, badgeStyle(style), ST_BRASS, radius);
        canvasFlush();
        renderPause(game, 20);
    }
    for (frame = 0; frame < BANNER_BLINKS * 2; frame++) {
        canvasFill(x0, ROW_RESULT, width, 1, ST_PLAIN);
        drawBanner(x0, width, fitted, badgeStyle(style),
                   (frame & 1) ? ST_BRASS : ST_GOLD_B, total);
        canvasFlush();
        renderPause(game, 70);
    }
}

void renderMessage(const char *msg,
                   int row,
                   MessageStyle style,
                   int padding) {
    int terminalWidth;
    int terminalHeight;
    int maxWidth;
    int x;

    renderInitColors();

    if (isatty(STDOUT_FILENO) &&
        !renderTerminalSizeOk(&terminalWidth,
                              &terminalHeight)) {
        renderTooSmallScreen(terminalWidth, terminalHeight);
        return;
    }

    if (isatty(STDOUT_FILENO) && canvasBegin()) {
        if (padding < 0) {
            padding = 0;
        }
        x = padding + 1;
        maxWidth = canvasWidth() - padding - 2;
        if (maxWidth < 8) {
            maxWidth = 8;
        }
        canvasFill(x, row, canvasWidth() - x + 1, 1, ST_PLAIN);
        canvasPutFit(x, row, maxWidth, msg, messageStyle(style));
        canvasTouchRow(row);
        canvasFlush();
    } else {
        printf("%s\n", msg);
        fflush(stdout);
    }
}
