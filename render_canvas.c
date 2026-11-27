#define _DEFAULT_SOURCE
#include "render_canvas.h"
#include "terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CANVAS_MAX_W 400
#define CANVAS_MAX_H 200

typedef struct {
    char g[6];
    RenderStyle st;
} Cell;

static Cell *cur;
static Cell *shadow;
static unsigned char *forced;
static int cw;
static int ch;
static int shadowValid;
static int needClear;

static char *outBuf;
static size_t outLen;
static size_t outCap;
static int outFailed;

/* ------------------------------------------------------------------ */
/* Glyph sets                                                          */
/* ------------------------------------------------------------------ */

static const Glyphs GLYPHS_UNICODE = {
    "\xe2\x95\x94", "\xe2\x95\x97", "\xe2\x95\x9a", "\xe2\x95\x9d",
    "\xe2\x95\x90", "\xe2\x95\x91",                                   /* ╔╗╚╝═║ */
    "\xe2\x95\x9f", "\xe2\x95\xa2", "\xe2\x94\x80",
    "\xe2\x95\xa1", "\xe2\x95\x9e",                                   /* ╟╢─╡╞ */
    "\xe2\x95\xad", "\xe2\x95\xae", "\xe2\x95\xb0", "\xe2\x95\xaf",
    "\xe2\x94\x80", "\xe2\x94\x82",                                   /* ╭╮╰╯─│ */
    {
        { "\xe2\x94\x8c", "\xe2\x94\x90", "\xe2\x94\x94", "\xe2\x94\x98",
          "\xe2\x94\x80", "\xe2\x94\x82" },                           /* J ┌┐└┘─│ */
        { "\xe2\x94\x8f", "\xe2\x94\x93", "\xe2\x94\x97", "\xe2\x94\x9b",
          "\xe2\x94\x81", "\xe2\x94\x83" },                           /* Q ┏┓┗┛━┃ */
        { "\xe2\x95\x94", "\xe2\x95\x97", "\xe2\x95\x9a", "\xe2\x95\x9d",
          "\xe2\x95\x90", "\xe2\x95\x91" },                           /* K ╔╗╚╝═║ */
    },
    { "\xe2\x99\xa5", "\xe2\x99\xa6", "\xe2\x99\xa3", "\xe2\x99\xa0" }, /* ♥♦♣♠ */
    "\xc2\xb7",                                                       /* · */
    "\xe2\x80\xba",                                                   /* › */
    "\xe2\x97\x86",                                                   /* ◆ */
    "\xe2\x97\x8f",                                                   /* ● */
    "\xe2\x96\xb6",                                                   /* ▶ */
    "\xe2\x98\x85",                                                   /* ★ */
    "\xe2\x94\x82",                                                   /* │ */
    "\xe2\x96\x88",                                                   /* █ */
    "\xe2\x96\x91",                                                   /* ░ */
    "\xe2\x95\xb1", "\xe2\x95\xb2",                                   /* ╱ ╲ */
    "\xe2\x96\x90", "\xe2\x96\x8c",                                   /* ▐ ▌ */
    "\xe2\x96\x92", "\xe2\x97\x8f",                                   /* ▒ ● */
    "\xe2\x96\xb2", "\xe2\x96\xbc",                                   /* ▲ ▼ */
    "\xe2\x9c\x93",                                                   /* ✓ */
    "\xe2\x97\x86",                                                   /* ◆ */
};

static const Glyphs GLYPHS_ASCII = {
    "+", "+", "+", "+", "=", "|",
    "+", "+", "-", "[", "]",
    "+", "+", "+", "+", "-", "|",
    {
        { "+", "+", "+", "+", "-", "|" },
        { "+", "+", "+", "+", "-", "|" },
        { "+", "+", "+", "+", "=", "|" },
    },
    { "H", "D", "C", "S" },
    " ",
    ">",
    "*",
    "o",
    ">",
    "*",
    "|",
    "#",
    ".",
    "/", "\\",
    "(", ")",
    "=", "o",
    "^", "v",
    "+",
    "*",
};

const Glyphs *renderGlyphs(void) {
    return renderUtf8Enabled() ? &GLYPHS_UNICODE : &GLYPHS_ASCII;
}

/* ------------------------------------------------------------------ */
/* UTF-8                                                               */
/* ------------------------------------------------------------------ */

static size_t utf8SeqLength(const unsigned char *p) {
    size_t need;
    size_t i;

    if (p[0] < 0x80) {
        return 1;
    }
    if (p[0] >= 0xC2 && p[0] <= 0xDF) {
        need = 2;
    } else if (p[0] >= 0xE0 && p[0] <= 0xEF) {
        need = 3;
    } else if (p[0] >= 0xF0 && p[0] <= 0xF4) {
        need = 4;
    } else {
        return 0;
    }
    for (i = 1; i < need; i++) {
        if ((p[i] & 0xC0) != 0x80) {
            return 0;
        }
    }
    return need;
}

int renderUtf8Length(const char *utf8) {
    const unsigned char *p;
    int count;
    size_t n;

    count = 0;
    p = (const unsigned char *)utf8;
    while (p != NULL && *p != '\0') {
        n = utf8SeqLength(p);
        p += n == 0 ? 1 : n;
        count++;
    }
    return count;
}

int canvasTextWidth(const char *utf8) {
    return renderUtf8Length(utf8);
}

/* ------------------------------------------------------------------ */
/* Canvas storage                                                      */
/* ------------------------------------------------------------------ */

static void setBlank(Cell *cell) {
    memset(cell->g, 0, sizeof(cell->g));
    cell->g[0] = ' ';
    cell->st = ST_PLAIN;
}

static void blankAll(Cell *cells, int count) {
    int i;

    for (i = 0; i < count; i++) {
        setBlank(&cells[i]);
    }
}

int canvasBegin(void) {
    int width;
    int height;

    if (!renderCursorAddressable()) {
        return 0;
    }
    terminalGetSize(&width, &height);
    if (width < 1 || height < 1) {
        return 0;
    }
    if (width > CANVAS_MAX_W) width = CANVAS_MAX_W;
    if (height > CANVAS_MAX_H) height = CANVAS_MAX_H;

    if (cur == NULL || width != cw || height != ch) {
        free(cur);
        free(shadow);
        free(forced);
        cur = (Cell *)malloc((size_t)width * (size_t)height * sizeof(Cell));
        shadow = (Cell *)malloc((size_t)width * (size_t)height *
                                sizeof(Cell));
        forced = (unsigned char *)calloc((size_t)height, 1);
        if (cur == NULL || shadow == NULL || forced == NULL) {
            free(cur);
            free(shadow);
            free(forced);
            cur = NULL;
            shadow = NULL;
            forced = NULL;
            cw = 0;
            ch = 0;
            return 0;
        }
        cw = width;
        ch = height;
        blankAll(cur, cw * ch);
        blankAll(shadow, cw * ch);
        shadowValid = 0;
        needClear = 1;
    }
    return 1;
}

int canvasWidth(void) { return cw; }
int canvasHeight(void) { return ch; }

void canvasBlank(void) {
    if (cur != NULL) {
        blankAll(cur, cw * ch);
    }
}

void canvasTouchRow(int y) {
    if (forced != NULL && y >= 1 && y <= ch) {
        forced[y - 1] = 1;
    }
}

void canvasInvalidate(void) {
    shadowValid = 0;
}

void canvasForceClear(void) {
    shadowValid = 0;
    needClear = 1;
}

static void putCell(int x, int y, const char *glyph, size_t length,
                    RenderStyle style) {
    Cell *cell;

    if (cur == NULL || x < 1 || y < 1 || x > cw || y > ch) {
        return;
    }
    cell = &cur[(y - 1) * cw + (x - 1)];
    memset(cell->g, 0, sizeof(cell->g));
    if (length == 0 || length > 4) {
        cell->g[0] = '?';
    } else {
        memcpy(cell->g, glyph, length);
    }
    cell->st = style;
}

void canvasPutGlyph(int x, int y, const char *glyph, RenderStyle style) {
    size_t n;

    if (glyph == NULL) {
        return;
    }
    n = utf8SeqLength((const unsigned char *)glyph);
    putCell(x, y, glyph, n, style);
}

int canvasPutText(int x, int y, const char *utf8, RenderStyle style) {
    const unsigned char *p;
    size_t n;
    int columns;

    if (utf8 == NULL) {
        return 0;
    }
    columns = 0;
    p = (const unsigned char *)utf8;
    while (*p != '\0') {
        n = utf8SeqLength(p);
        putCell(x + columns, y, (const char *)p, n, style);
        p += n == 0 ? 1 : n;
        columns++;
    }
    return columns;
}

int canvasPutTextN(int x, int y, int maxCols, const char *utf8,
                   RenderStyle style) {
    const unsigned char *p;
    size_t n;
    int columns;

    if (utf8 == NULL) {
        return 0;
    }
    columns = 0;
    p = (const unsigned char *)utf8;
    while (*p != '\0' && columns < maxCols) {
        n = utf8SeqLength(p);
        putCell(x + columns, y, (const char *)p, n, style);
        p += n == 0 ? 1 : n;
        columns++;
    }
    return columns;
}

int canvasPutFit(int x, int y, int maxCols, const char *utf8,
                 RenderStyle style) {
    const unsigned char *p;
    size_t n;
    int width;
    int limit;
    int columns;

    if (utf8 == NULL || maxCols <= 0) {
        return 0;
    }
    width = renderUtf8Length(utf8);
    if (width <= maxCols) {
        return canvasPutText(x, y, utf8, style);
    }
    limit = maxCols >= 4 ? maxCols - 3 : maxCols;
    columns = 0;
    p = (const unsigned char *)utf8;
    while (*p != '\0' && columns < limit) {
        n = utf8SeqLength(p);
        putCell(x + columns, y, (const char *)p, n, style);
        p += n == 0 ? 1 : n;
        columns++;
    }
    if (maxCols >= 4) {
        columns += canvasPutText(x + columns, y, "...", style);
    }
    return columns;
}

int canvasPutCentered(int x, int w, int y, const char *utf8,
                      RenderStyle style) {
    int width;
    int start;

    width = renderUtf8Length(utf8);
    start = x + (w - width) / 2;
    if (start < x) {
        start = x;
    }
    canvasPutText(start, y, utf8, style);
    return start;
}

void canvasFill(int x, int y, int w, int h, RenderStyle style) {
    int row;
    int col;

    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            putCell(x + col, y + row, " ", 1, style);
        }
    }
}

void canvasHLine(int x, int y, int w, const char *glyph, RenderStyle style) {
    int i;
    size_t n;

    n = glyph != NULL ? utf8SeqLength((const unsigned char *)glyph) : 0;
    for (i = 0; i < w; i++) {
        putCell(x + i, y, glyph, n, style);
    }
}

void canvasVLine(int x, int y, int h, const char *glyph, RenderStyle style) {
    int i;
    size_t n;

    n = glyph != NULL ? utf8SeqLength((const unsigned char *)glyph) : 0;
    for (i = 0; i < h; i++) {
        putCell(x, y + i, glyph, n, style);
    }
}

void canvasBox(int x, int y, int w, int hgt, const char *title,
               RenderStyle frame, RenderStyle fill, RenderStyle titleStyle) {
    const Glyphs *glyphs;
    char padded[160];
    int titleW;
    int row;

    if (w < 3 || hgt < 2) {
        return;
    }
    glyphs = renderGlyphs();
    canvasFill(x, y, w, hgt, fill);
    canvasPutGlyph(x, y, glyphs->fTL, frame);
    canvasHLine(x + 1, y, w - 2, glyphs->fH, frame);
    canvasPutGlyph(x + w - 1, y, glyphs->fTR, frame);
    for (row = 1; row < hgt - 1; row++) {
        canvasPutGlyph(x, y + row, glyphs->fV, frame);
        canvasPutGlyph(x + w - 1, y + row, glyphs->fV, frame);
    }
    canvasPutGlyph(x, y + hgt - 1, glyphs->fBL, frame);
    canvasHLine(x + 1, y + hgt - 1, w - 2, glyphs->fH, frame);
    canvasPutGlyph(x + w - 1, y + hgt - 1, glyphs->fBR, frame);
    if (title != NULL && title[0] != '\0') {
        snprintf(padded, sizeof(padded), " %s ", title);
        titleW = renderUtf8Length(padded);
        if (titleW > w - 6) {
            titleW = w - 6;
        }
        if (titleW > 0) {
            int tx;

            tx = x + (w - titleW) / 2;
            canvasPutGlyph(tx - 1, y, glyphs->capL, frame);
            canvasPutFit(tx, y, titleW, padded, titleStyle);
            canvasPutGlyph(tx + titleW, y, glyphs->capR, frame);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Output                                                              */
/* ------------------------------------------------------------------ */

static void outAppend(const char *data, size_t length) {
    char *grown;
    size_t wanted;
    size_t required;

    if (outFailed) {
        return;
    }
    if (outLen == (size_t)-1 ||
        length > (size_t)-1 - outLen - 1) {
        outFailed = 1;
        return;
    }
    required = outLen + length + 1;
    if (required > outCap) {
        wanted = outCap == 0 ? 8192 : outCap;
        while (wanted < required) {
            if (wanted > (size_t)-1 / 2) {
                outFailed = 1;
                return;
            }
            wanted *= 2;
        }
        grown = (char *)realloc(outBuf, wanted);
        if (grown == NULL) {
            outFailed = 1;
            return;
        }
        outBuf = grown;
        outCap = wanted;
    }
    memcpy(outBuf + outLen, data, length);
    outLen += length;
}

static void outString(const char *text) {
    outAppend(text, strlen(text));
}

static int isBlankCell(const Cell *cell) {
    return cell->g[0] == ' ' && cell->g[1] == '\0' && cell->st == ST_PLAIN;
}

static void emitCursor(int row, int column) {
    char seq[32];
    int n;

    n = snprintf(seq, sizeof(seq), "\033[%d;%dH", row, column);
    outAppend(seq, (size_t)n);
}

static void emitCells(const Cell *cells, int lo, int hi,
                      RenderStyle *active) {
    char sgr[64];
    int x;

    for (x = lo; x <= hi; x++) {
        if (cells[x].st != *active) {
            if (renderStyleSgr(cells[x].st, sgr, sizeof(sgr)) > 0) {
                outString(sgr);
            }
            *active = cells[x].st;
        }
        outString(cells[x].g);
    }
}

static void emitReset(RenderStyle *active) {
    char sgr[64];

    if (*active != ST_PLAIN) {
        if (renderStyleSgr(ST_PLAIN, sgr, sizeof(sgr)) > 0) {
            outString(sgr);
        }
        *active = ST_PLAIN;
    }
}

void canvasFlushEx(void (*extra)(const void *ctx), const void *ctx) {
    RenderStyle active;
    int y;
    int x;
    int lo;
    int hi;
    int last;
    int full;
    const Cell *row;
    const Cell *old;

    if (cur == NULL) {
        if (extra != NULL) {
            extra(ctx);
        }
        fflush(stdout);
        return;
    }

    outLen = 0;
    outFailed = 0;
    outString("\033[?2026h\033[?7l");
    if (needClear) {
        if (renderAnsiEnabled()) {
            outString("\033[0m");
        }
        outString("\033[2J");
        needClear = 0;
        shadowValid = 0;
    }
    if (renderAnsiEnabled()) {
        outString("\033[0m");
    }
    active = ST_PLAIN;

    for (y = 0; y < ch; y++) {
        row = &cur[y * cw];
        old = &shadow[y * cw];
        full = forced[y] || !shadowValid;
        if (full) {
            last = cw - 1;
            while (last >= 0 && isBlankCell(&row[last])) {
                last--;
            }
            emitCursor(y + 1, 1);
            if (last >= 0) {
                emitCells(row, 0, last, &active);
            }
            emitReset(&active);
            if (last < cw - 1) {
                outString("\033[K");
            }
        } else {
            lo = -1;
            hi = -1;
            for (x = 0; x < cw; x++) {
                if (memcmp(&row[x], &old[x], sizeof(Cell)) != 0) {
                    if (lo < 0) {
                        lo = x;
                    }
                    hi = x;
                }
            }
            if (lo < 0) {
                continue;
            }
            emitCursor(y + 1, lo + 1);
            emitCells(row, lo, hi, &active);
        }
    }
    emitReset(&active);

    if (outFailed) {
        outLen = 0;
        outFailed = 0;
        shadowValid = 0;
        return;
    }

    if (fwrite(outBuf, 1, outLen, stdout) != outLen) {
        shadowValid = 0;
        return;
    }
    memcpy(shadow, cur, (size_t)cw * (size_t)ch * sizeof(Cell));
    memset(forced, 0, (size_t)ch);
    shadowValid = 1;
    if (extra != NULL) {
        extra(ctx);
    }
    fputs("\033[?7h\033[?2026l", stdout);
    fflush(stdout);
}

void canvasFlush(void) {
    canvasFlushEx(NULL, NULL);
}
