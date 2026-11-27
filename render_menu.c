#define _DEFAULT_SOURCE
#include "render_menu.h"
#include "render_canvas.h"
#include "render_control.h"
#include "render_scene.h"
#include "render_terminal.h"
#include "render_style.h"
#include "render_layout.h"
#include "terminal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ================================================================== */
/* Title screen                                                        */
/*                                                                     */
/* Shown by renderMenu() whenever no table is on screen (that is, after */
/* renderResetScreen()), e.g. for the main menu at start-up. Menus     */
/* opened over the table keep the compact framed overlay below.        */
/* ================================================================== */

#define LOGO_LETTERS 5
#define LOGO_ROWS 5
#define SPLASH_TAGLINES 5

static const char *const LOGO_FONT[4][LOGO_ROWS] = {
    { "#####", "  #  ", "  #  ", "  #  ", "  #  " },   /* T */
    { "#   #", " # # ", "  #  ", "  #  ", "  #  " },   /* Y */
    { "#####", "    #", "#####", "#    ", "#####" },   /* 2 */
    { "  #  ", " ##  ", "  #  ", "  #  ", " ### " },   /* 1 */
};
static const int LOGO_ORDER[LOGO_LETTERS] = { 0, 0, 1, 2, 3 };

static const char *const SPLASH_QUOTES[SPLASH_TAGLINES] = {
    "The house always deals first.",
    "Every floor raises the stakes.",
    "Beat the dealer. Break the house.",
    "One more hand. Always one more hand.",
    "Climb the tower, one hand at a time.",
};

#define SUBTITLE_TEXT "B L A C K J A C K   R O G U E L I K E"

typedef struct {
    int width;
    int height;
    int tier;          /* 0 tall, 1 medium, 2 tiny */
    int stride;        /* rows per menu item (2 with a tagline) */
    int logoX;
    int logoY;
    int logoScale;     /* columns per logo pixel: 1 or 2 */
    int logoW;
    int ornamentY;
    int subtitleY;
    int quoteY;
    int px;
    int py;
    int pw;
    int ph;
    int itemY;         /* row of the first item */
    int hintY;
    int cards;         /* 1 when the playing cards flank the panel */
    int cardLX;
    int cardLY;
    int cardRX;
    int cardRY;
} SplashLayout;

typedef struct {
    int cardL;         /* flight progress 0..256, -1 not launched */
    int cardR;
    int flip;          /* -1 face down, 0..8 turning, 9 face up */
    int logoCols;
    int ornament;
    int subChars;
    int quoteChars;
    int panelRows;
    int itemsShown;
    int barReveal;     /* 0..256 selection highlight sweep */
    int selected;
} SplashFrame;

static int splashActive;
static int splashWidth;
static int splashHeight;
static int splashCount;
static int splashSelected = -1;
static int introsPlayed;
static int quoteIndex = -1;

void renderMenuForgetSplash(void) {
    splashActive = 0;
}

static void menuPause(int ms) {
    if (ms > 0 && isatty(STDOUT_FILENO) && renderMotionEnabled()) {
        usleep((unsigned int)ms * 1000U);
    }
}

static unsigned int scatterHash(int x, int y) {
    return ((unsigned int)x * 2654435761u) ^ ((unsigned int)y * 40503u) ^
           ((unsigned int)(x + y) * 97u);
}

/* Faint dots and the odd suit: the "dust" of the casino floor. */
void renderBackdrop(int x, int y, int w, int h) {
    const Glyphs *glyphs;
    unsigned int hash;
    int col;
    int row;

    glyphs = renderGlyphs();
    for (row = y; row < y + h; row++) {
        for (col = x; col < x + w; col++) {
            hash = scatterHash(col, row);
            if (hash % 211u == 0u) {
                canvasPutGlyph(col, row, glyphs->suit[(hash / 211u) % 4u],
                               ST_BRASS | RSTYLE_DIM);
            } else if (renderUtf8Enabled() && hash % 29u == 0u) {
                canvasPutGlyph(col, row, glyphs->dot,
                               ST_BRASS | RSTYLE_DIM);
            }
        }
    }
}

static int hasWord(const char *text, const char *word) {
    size_t length;
    size_t index;

    length = strlen(word);
    for (; *text != '\0'; text++) {
        for (index = 0; index < length; index++) {
            if (text[index] == '\0' ||
                tolower((unsigned char)text[index]) != word[index]) {
                break;
            }
        }
        if (index == length) {
            return 1;
        }
    }
    return 0;
}

static const char *taglineFor(const char *label, int disabled) {
    if (hasWord(label, "continue") || hasWord(label, "resume") ||
        hasWord(label, "load")) {
        return disabled ? "No saved run yet."
                        : "Back to the table, right where you left.";
    }
    if (hasWord(label, "new")) {
        return "Take a seat. The house is shuffling.";
    }
    if (hasWord(label, "stat")) {
        return "Floors climbed, bosses felled, marks won.";
    }
    if (hasWord(label, "help") || hasWord(label, "rule")) {
        return "The house rules, in brief.";
    }
    if (hasWord(label, "quit") || hasWord(label, "exit")) {
        return "Cash out and walk away.";
    }
    return NULL;
}

static int computeSplash(int width, int height, const char *title,
                         const MenuItem *items, int itemCount,
                         SplashLayout *layout) {
    int maxLabel;
    int labelWidth;
    int titleWidth;
    int index;
    int total;
    int top;
    int pad;
    int y;

    memset(layout, 0, sizeof(*layout));
    layout->width = width;
    layout->height = height;

    maxLabel = 0;
    for (index = 0; index < itemCount; index++) {
        labelWidth = canvasTextWidth(items[index].label);
        if (labelWidth > maxLabel) {
            maxLabel = labelWidth;
        }
    }
    titleWidth = title != NULL ? canvasTextWidth(title) : 0;

    if (width >= 50 && height >= 19 + 2 * itemCount) {
        layout->tier = 0;
    } else if (width >= 40 && height >= 19 + itemCount) {
        layout->tier = 1;
    } else {
        layout->tier = 2;
    }
    layout->stride = layout->tier == 0 ? 2 : 1;
    pad = layout->tier == 2 ? 0 : 1;

    layout->pw = maxLabel + 14;
    if (layout->tier == 0 && layout->pw < 54) {
        layout->pw = 54;
    }
    if (titleWidth + 10 > layout->pw) {
        layout->pw = titleWidth + 10;
    }
    if (layout->pw > width - 2) {
        layout->pw = width - 2;
    }
    if (layout->pw < 12) {
        layout->pw = 12;
    }
    layout->px = (width - layout->pw) / 2 + 1;
    layout->ph = 2 + 2 * pad + itemCount * layout->stride;

    if (layout->tier < 2) {
        total = 11 + layout->ph + 2;
    } else {
        total = 3 + layout->ph + 2;
    }
    top = (height - total) * 2 / 5 + 1;
    if (top < 1) {
        top = 1;
    }
    y = top;
    layout->logoScale = width >= 72 ? 2 : 1;
    layout->logoW = layout->logoScale == 2 ? 62 : 33;
    if (layout->tier < 2) {
        layout->logoX = (width - layout->logoW) / 2 + 1;
        layout->logoY = y;
        layout->ornamentY = y + 7;
        layout->subtitleY = y + 8;
        layout->quoteY = y + 9;
        y += 11;
    } else {
        layout->logoX = (width - 9) / 2 + 1;
        layout->logoY = y;
        layout->subtitleY = y + 1;
        y += 3;
    }
    layout->py = y;
    layout->itemY = y + 1 + pad;
    layout->hintY = y + layout->ph + 1;

    layout->cardLX = layout->px - 12;
    layout->cardRX = layout->px + layout->pw + 1;
    layout->cards = layout->tier == 0 &&
                    layout->cardLX >= 2 &&
                    layout->cardRX + 10 <= width - 1;
    layout->cardLY = layout->py + 1;
    layout->cardRY = layout->py + layout->ph - 8;
    if (layout->cardRY < layout->py) {
        layout->cardRY = layout->py;
    }
    return 1;
}

/* ---- pieces ------------------------------------------------------ */

static void drawLogoBig(const SplashLayout *layout, int cols) {
    static const RenderStyle GRADIENT[LOGO_ROWS] = {
        ST_GOLD_B, ST_GOLD_B, ST_GOLD, ST_GOLD, ST_BRASS
    };
    const char *block;
    const char *shade;
    int scale;
    int stride;
    int pass;
    int row;
    int letter;
    int cell;
    int part;
    int column;

    block = renderUtf8Enabled() ? "\xe2\x96\x88" : "#";
    shade = renderUtf8Enabled() ? "\xe2\x96\x92" : ".";
    scale = layout->logoScale;
    stride = 5 * scale + (scale == 2 ? 3 : 2);
    for (pass = 0; pass < 2; pass++) {
        for (row = 0; row < LOGO_ROWS; row++) {
            for (letter = 0; letter < LOGO_LETTERS; letter++) {
                for (cell = 0; cell < 5; cell++) {
                    if (LOGO_FONT[LOGO_ORDER[letter]][row][cell] != '#') {
                        continue;
                    }
                    for (part = 0; part < scale; part++) {
                        column = letter * stride + cell * scale + part;
                        if (column >= cols) {
                            continue;
                        }
                        if (pass == 0) {
                            canvasPutGlyph(layout->logoX + column + scale,
                                           layout->logoY + row + 1, shade,
                                           ST_BRASS | RSTYLE_DIM);
                        } else {
                            canvasPutGlyph(layout->logoX + column,
                                           layout->logoY + row, block,
                                           (cols < layout->logoW &&
                                            column >= cols - 2)
                                               ? ST_STRONG : GRADIENT[row]);
                        }
                    }
                }
            }
        }
    }
}

static void drawOrnament(const SplashLayout *layout) {
    const Glyphs *glyphs;
    static const int ORDER[4] = { 3, 0, 2, 1 };
    int x;
    int index;

    glyphs = renderGlyphs();
    x = layout->logoX;
    canvasHLine(x, layout->ornamentY, layout->logoW, glyphs->rule, ST_BRASS);
    x += layout->logoW / 2 - 4;
    canvasFill(x, layout->ornamentY, 9, 1, ST_PLAIN);
    for (index = 0; index < 4; index++) {
        canvasPutGlyph(x + 1 + index * 2, layout->ornamentY,
                       glyphs->suit[ORDER[index]],
                       (ORDER[index] == 0 || ORDER[index] == 1)
                           ? ST_BAD : ST_STRONG);
    }
}

static void drawSubtitle(const SplashLayout *layout, int chars) {
    const char *text;
    int split;
    int width;
    int x;
    int shown;

    if (layout->width >= 40) {
        text = SUBTITLE_TEXT;   /* letter-spaced */
        split = 17;
    } else {
        text = "BLACKJACK ROGUELIKE";
        split = 10;
        chars = chars * 19 / (int)strlen(SUBTITLE_TEXT);
    }
    width = (int)strlen(text);
    x = (layout->width - width) / 2 + 1;
    shown = chars < width ? chars : width;
    canvasPutTextN(x, layout->subtitleY, shown < split ? shown : split,
                   text, ST_STRONG);
    if (shown > split) {
        canvasPutTextN(x + split, layout->subtitleY, shown - split,
                       text + split, ST_GOLD);
    }
}

static void drawQuote(const SplashLayout *layout, int chars) {
    const char *quote;
    int width;
    int x;

    if (quoteIndex < 0) {
        /* one line per launch, so it never changes mid-animation */
        quoteIndex = (int)((unsigned int)time(NULL) % SPLASH_TAGLINES);
    }
    quote = SPLASH_QUOTES[quoteIndex];
    width = (int)strlen(quote);
    x = (layout->width - width) / 2 + 1;
    canvasPutTextN(x, layout->quoteY, chars, quote, ST_MUTED);
}

/* Writes text, switching style at column splitX. */
static void putSplit(int x, int y, const char *text, RenderStyle inside,
                     RenderStyle outside, int splitX) {
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
        canvasPutGlyph(column, y, glyph, column < splitX ? inside : outside);
        p += length;
        column++;
    }
}

static void drawItem(const SplashLayout *layout, const MenuItem *item,
                     int selected, int disabled, int reveal, int row) {
    const Glyphs *glyphs;
    const char *tagline;
    char key[8];
    char label[160];
    int innerLeft;
    int innerWidth;
    int barEnd;
    int labelX;
    int labelWidth;

    glyphs = renderGlyphs();
    innerLeft = layout->px + 1;
    innerWidth = layout->pw - 2;
    labelX = layout->px + 10;
    labelWidth = layout->pw - 12;
    snprintf(key, sizeof(key), "[%c]", toupper((unsigned char)item->hotkey));
    snprintf(label, sizeof(label), "%s", item->label);

    if (disabled) {
        canvasPutText(layout->px + 5, row, key, ST_FELT_MUTED);
        canvasPutFit(labelX, row, labelWidth, label, ST_FELT_MUTED);
    } else if (selected) {
        barEnd = innerLeft + innerWidth * reveal / 256;
        canvasFill(innerLeft, row, barEnd - innerLeft, 1, ST_SEL);
        putSplit(layout->px + 3, row, glyphs->pointer, ST_SEL,
                 ST_FELT_GOLD, barEnd);
        putSplit(layout->px + 5, row, key, ST_SEL_KEY, ST_FELT_GOLD, barEnd);
        canvasPutFit(labelX, row, labelWidth, label,
                     reveal >= 256 ? ST_SEL : ST_FELT_TEXT);
        if (reveal < 256) {
            /* part of the label is already under the bar */
            putSplit(labelX, row, label, ST_SEL, ST_FELT_TEXT, barEnd);
        }
        if (!renderAnsiEnabled()) {
            canvasPutGlyph(layout->px + layout->pw - 4, row,
                           renderUtf8Enabled() ? "\xe2\x97\x80" : "<",
                           ST_PLAIN);
        }
    } else {
        canvasPutText(layout->px + 5, row, key, ST_FELT_GOLD);
        canvasPutFit(labelX, row, labelWidth, label, ST_FELT_TEXT);
    }

    if (layout->stride == 2) {
        tagline = taglineFor(item->label, disabled);
        if (tagline != NULL) {
            canvasPutFit(labelX, row + 1, labelWidth, tagline,
                         (selected && !disabled) ? ST_FELT_TEXT
                                                 : ST_FELT_MUTED);
        }
    }
}

static void drawPanel(const SplashLayout *layout, const char *title,
                      const MenuItem *items, int itemCount,
                      int disabledMask, const SplashFrame *frame) {
    int rows;
    int top;
    int index;

    if (frame->panelRows <= 0) {
        return;
    }
    rows = frame->panelRows < layout->ph ? frame->panelRows : layout->ph;
    if (rows < 3) {
        rows = 3;
    }
    top = layout->py + (layout->ph - rows) / 2;
    canvasBox(layout->px, top, layout->pw, rows,
              rows == layout->ph ? title : NULL, ST_BRASS, ST_FELT_TEXT,
              ST_GOLD_B);
    if (rows < layout->ph) {
        return;
    }
    for (index = 0; index < itemCount && index < frame->itemsShown;
         index++) {
        drawItem(layout, &items[index], index == frame->selected,
                 (disabledMask & (1 << index)) != 0,
                 index == frame->selected ? frame->barReveal : 256,
                 layout->itemY + index * layout->stride);
    }
}

static void drawCards(const SplashLayout *layout, const SplashFrame *frame) {
    static const int WIDTHS[9] = { 9, 7, 5, 3, 1, 3, 5, 7, 9 };
    TableLayout sprite;
    Card ace;
    Card king;
    int x;
    int y;
    int t;
    int scale;
    int down;

    if (!layout->cards || frame->cardL < 0) {
        return;
    }
    memset(&sprite, 0, sizeof(sprite));
    sprite.cardW = 11;
    sprite.cardH = 7;
    memset(&ace, 0, sizeof(ace));
    memset(&king, 0, sizeof(king));
    ace.rank = 1;
    ace.suit = SPADES;
    king.rank = 13;
    king.suit = HEARTS;

    scale = 0;
    down = 1;
    if (frame->flip >= 0 && frame->flip < 9) {
        scale = WIDTHS[frame->flip];
        down = frame->flip < 4;
    } else if (frame->flip >= 9) {
        down = 0;
    }

    t = frame->cardL;
    x = layout->cardLX + (layout->width / 2 - 5 - layout->cardLX) *
                             (256 - t) / 256;
    y = layout->cardLY - (layout->cardLY - 1) * (256 - t) / 256;
    renderCardSprite(&sprite, &ace, down, x, y, scale, ST_CARD_EDGE);

    if (frame->cardR >= 0) {
        t = frame->cardR;
        x = layout->cardRX + (layout->width / 2 - 5 - layout->cardRX) *
                                 (256 - t) / 256;
        y = layout->cardRY - (layout->cardRY - 1) * (256 - t) / 256;
        renderCardSprite(&sprite, &king, down, x, y, scale, ST_CARD_EDGE);
    }
}

static void drawFooter(const SplashLayout *layout) {
    static const char *const UNI[3] = {
        "\xe2\x86\x91 \xe2\x86\x93  select     Enter  confirm     hotkeys  jump",
        "\xe2\x86\x91\xe2\x86\x93 select   Enter confirm",
        "Enter confirm",
    };
    static const char *const ASC[3] = {
        "Up/Down select     Enter confirm     hotkeys jump",
        "Up/Down select   Enter confirm",
        "Enter confirm",
    };
    const char *const *set;
    const char *hint;
    int pick;

    set = renderUtf8Enabled() ? UNI : ASC;
    pick = 0;
    while (pick < 2 && canvasTextWidth(set[pick]) > layout->width - 2) {
        pick++;
    }
    hint = set[pick];
    canvasFill(1, layout->hintY, layout->width, 1, ST_PLAIN);
    canvasPutCentered(1, layout->width, layout->hintY, hint, ST_MUTED);
#ifdef TTY21_VERSION
    {
        char version[32];
        int x;

        snprintf(version, sizeof(version), "v%s", TTY21_VERSION);
        x = layout->width - (int)strlen(version);
        if (x < 1) {
            x = 1;
        }
        canvasPutText(x, layout->height, version, ST_BRASS | RSTYLE_DIM);
    }
#endif
}

static void composeSplash(const SplashLayout *layout, const char *title,
                          const MenuItem *items, int itemCount,
                          int disabledMask, const SplashFrame *frame) {
    canvasBlank();
    renderBackdrop(1, 1, layout->width, layout->height);
    drawCards(layout, frame);
    canvasFill(1, layout->subtitleY, layout->width, 1, ST_PLAIN);
    if (layout->tier < 2) {
        canvasFill(1, layout->ornamentY, layout->width, 1, ST_PLAIN);
        canvasFill(1, layout->quoteY, layout->width, 1, ST_PLAIN);
        drawLogoBig(layout, frame->logoCols);
        if (frame->ornament) {
            drawOrnament(layout);
        }
        drawQuote(layout, frame->quoteChars);
    } else {
        canvasPutCentered(1, layout->width, layout->logoY, "T T Y 2 1",
                          frame->logoCols > 0 ? ST_GOLD_B : ST_PLAIN);
    }
    drawSubtitle(layout, frame->subChars);
    drawPanel(layout, title, items, itemCount, disabledMask, frame);
    if (frame->itemsShown >= itemCount) {
        drawFooter(layout);
    }
}

static void finalFrame(SplashFrame *frame, const SplashLayout *layout,
                       int itemCount, int selected) {
    frame->cardL = 256;
    frame->cardR = 256;
    frame->flip = 9;
    frame->logoCols = layout->logoW;
    frame->ornament = 1;
    frame->subChars = (int)strlen(SUBTITLE_TEXT);
    frame->quoteChars = 80;
    frame->panelRows = layout->ph;
    frame->itemsShown = itemCount;
    frame->barReveal = 256;
    frame->selected = selected;
}

static int easeOut256(int step, int steps) {
    int inverse;

    if (step <= 0) {
        return 0;
    }
    if (step >= steps) {
        return 256;
    }
    inverse = 256 - (step * 256) / steps;
    return 256 - (inverse * inverse * inverse) / 65536;
}

static void present(const SplashLayout *layout, const char *title,
                    const MenuItem *items, int itemCount, int disabledMask,
                    const SplashFrame *frame, int ms) {
    composeSplash(layout, title, items, itemCount, disabledMask, frame);
    canvasFlush();
    menuPause(ms);
}

static void playIntro(const SplashLayout *layout, const char *title,
                      const MenuItem *items, int itemCount,
                      int disabledMask, int selected, int full) {
    SplashFrame frame;
    int step;
    int rows;

    memset(&frame, 0, sizeof(frame));
    frame.cardL = -1;
    frame.cardR = -1;
    frame.flip = -1;
    frame.selected = selected;
    frame.barReveal = 256;

    if (full) {
        if (layout->cards) {
            for (step = 1; step <= 8; step++) {
                frame.cardL = easeOut256(step, 6);
                frame.cardR = step > 2 ? easeOut256(step - 2, 6) : -1;
                present(layout, title, items, itemCount, disabledMask,
                        &frame, 22);
            }
        }
        for (step = 1; step <= 9; step++) {
            frame.logoCols = layout->logoW * step / 9;
            present(layout, title, items, itemCount, disabledMask, &frame,
                    24);
        }
        frame.ornament = 1;
        for (step = 1; step <= 8; step++) {
            frame.subChars = (int)strlen(SUBTITLE_TEXT) * step / 8;
            frame.quoteChars = 80 * step / 8;
            present(layout, title, items, itemCount, disabledMask, &frame,
                    24);
        }
    } else {
        finalFrame(&frame, layout, itemCount, selected);
        frame.panelRows = 0;
        frame.itemsShown = 0;
        if (layout->cards) {
            frame.flip = 9;
        }
    }
    frame.cardL = layout->cards ? 256 : -1;
    frame.cardR = layout->cards ? 256 : -1;
    frame.logoCols = layout->logoW;
    frame.ornament = 1;
    frame.subChars = (int)strlen(SUBTITLE_TEXT);
    frame.quoteChars = 80;

    for (step = 1; step <= 4; step++) {
        rows = 3 + (layout->ph - 3) * step / 4;
        frame.panelRows = rows;
        present(layout, title, items, itemCount, disabledMask, &frame, 20);
    }
    frame.panelRows = layout->ph;
    for (step = 1; step <= itemCount; step++) {
        frame.itemsShown = step;
        present(layout, title, items, itemCount, disabledMask, &frame,
                full ? 55 : 25);
    }
    if (full && layout->cards) {
        for (step = 0; step <= 9; step++) {
            frame.flip = step;
            present(layout, title, items, itemCount, disabledMask, &frame,
                    26);
        }
    }
}

static void renderSplash(const char *title, const MenuItem *items,
                         int itemCount, int disabledMask,
                         int selectedIndex) {
    SplashLayout layout;
    SplashFrame frame;
    int width;
    int height;
    int step;
    int needFull;
    int animate;

    width = canvasWidth();
    height = canvasHeight();
    computeSplash(width, height, title, items, itemCount, &layout);
    renderHideCursor();

    animate = renderMotionEnabled() && isatty(STDOUT_FILENO);
    needFull = !splashActive || width != splashWidth ||
               height != splashHeight || itemCount != splashCount;

    if (needFull) {
        canvasBlank();
        canvasInvalidate();
        if (animate) {
            playIntro(&layout, title, items, itemCount, disabledMask,
                      selectedIndex, introsPlayed == 0);
        }
        introsPlayed++;
        splashActive = 1;
        splashWidth = width;
        splashHeight = height;
        splashCount = itemCount;
    } else if (animate && selectedIndex != splashSelected) {
        /* sweep the highlight across the new row */
        for (step = 1; step <= 3; step++) {
            finalFrame(&frame, &layout, itemCount, selectedIndex);
            frame.barReveal = 256 * step / 3;
            drawPanel(&layout, title, items, itemCount, disabledMask,
                      &frame);
            canvasFlush();
            menuPause(18);
        }
    }

    finalFrame(&frame, &layout, itemCount, selectedIndex);
    if (needFull) {
        composeSplash(&layout, title, items, itemCount, disabledMask,
                      &frame);
    } else {
        drawPanel(&layout, title, items, itemCount, disabledMask, &frame);
    }
    canvasFlush();
    splashSelected = selectedIndex;
}

/* ================================================================== */
/* Menu over the table                                                 */
/* ================================================================== */

static int lastMenuBottom;

/* Prints a framed menu block with disabled entries dimmed. The selected
 * entry is a solid brass bar. */
static void renderMenuOverlay(const char *title,
                const MenuItem *items,
                int itemCount,
                int disabledMask,
                int selectedIndex,
                int startRow) {
    const Glyphs *glyphs;
    char line[160];
    int terminalWidth;
    int terminalHeight;
    int itemIndex;
    int isDisabled;
    int labelWidth;
    int maxLabel;
    int boxWidth;
    int boxX;
    int regionX;
    int regionW;
    int innerX;
    int innerW;
    int bottom;
    int row;
    int top;

    renderInitColors();
    terminalGetSize(&terminalWidth, &terminalHeight);

    if (isatty(STDOUT_FILENO) &&
        !renderTerminalSizeOk(&terminalWidth,
                              &terminalHeight)) {
        renderTooSmallScreen(terminalWidth, terminalHeight);
        return;
    }

    if (isatty(STDOUT_FILENO) &&
        terminalWidth >= MIN_TERMINAL_WIDTH &&
        terminalHeight >= MIN_TERMINAL_HEIGHT &&
        canvasBegin()) {
        glyphs = renderGlyphs();
        renderHideCursor();

        maxLabel = 0;
        for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
            labelWidth = canvasTextWidth(items[itemIndex].label);
            if (labelWidth > maxLabel) {
                maxLabel = labelWidth;
            }
        }
        boxWidth = maxLabel + 12;
        if (canvasTextWidth(title) + 8 > boxWidth) {
            boxWidth = canvasTextWidth(title) + 8;
        }
        if (boxWidth > terminalWidth - 2) {
            boxWidth = terminalWidth - 2;
        }
        renderHudRegion(&regionX, &regionW);
        if (regionW < boxWidth) {
            regionX = 1;
            regionW = terminalWidth;
        }
        boxX = regionX + (regionW - boxWidth) / 2;
        top = startRow - 1;
        bottom = startRow + itemCount;

        /* wipe whatever an earlier, taller menu left behind */
        for (row = top; row <= (bottom > lastMenuBottom ? bottom
                                                        : lastMenuBottom);
             row++) {
            canvasFill(1, row, canvasWidth(), 1, ST_PLAIN);
            canvasTouchRow(row);
        }
        lastMenuBottom = bottom;

        canvasBox(boxX, top, boxWidth, itemCount + 2, title, ST_BRASS,
                  ST_PLAIN, ST_GOLD_B);
        innerX = boxX + 2;
        innerW = boxWidth - 4;

        for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
            isDisabled = (disabledMask & (1 << itemIndex)) != 0;
            row = startRow + itemIndex;
            if (isDisabled) {
                snprintf(line, sizeof(line), "  [%c]  %s",
                         items[itemIndex].hotkey, items[itemIndex].label);
                canvasPutFit(innerX, row, innerW, line, ST_MUTED);
            } else if (itemIndex == selectedIndex) {
                canvasFill(innerX - 1, row, innerW + 2, 1, ST_SEL);
                canvasPutText(innerX, row, glyphs->pointer, ST_SEL);
                snprintf(line, sizeof(line), "[%c]", items[itemIndex].hotkey);
                canvasPutText(innerX + 2, row, line, ST_SEL_KEY);
                canvasPutFit(innerX + 7, row, innerW - 7,
                             items[itemIndex].label, ST_SEL);
            } else {
                snprintf(line, sizeof(line), "[%c]", items[itemIndex].hotkey);
                canvasPutText(innerX + 2, row, line, ST_GOLD);
                canvasPutFit(innerX + 7, row, innerW - 7,
                             items[itemIndex].label, ST_PLAIN);
            }
        }
        canvasFlush();
    } else {
        {
            char header[512];

            formatPlainScreenHeader(header, sizeof(header),
                                    terminalWidth, title);
            printf("%s\n", header);
        }

        for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
            char plain[512];

            isDisabled = (disabledMask & (1 << itemIndex)) != 0;
            formatPlainMenuItem(plain,
                                sizeof(plain),
                                items[itemIndex].hotkey,
                                items[itemIndex].label,
                                itemIndex == selectedIndex,
                                isDisabled);
            printf("%s\n", plain);
        }

        fflush(stdout);
    }
}


/* Prints a menu block. Without a table on screen (main menu) it is the
 * title screen; over the table it is the compact framed overlay. */
void renderMenu(const char *title,
                const MenuItem *items,
                int itemCount,
                int disabledMask,
                int selectedIndex,
                int startRow) {
    int terminalWidth;
    int terminalHeight;

    renderInitColors();
    terminalGetSize(&terminalWidth, &terminalHeight);

    if (isatty(STDOUT_FILENO) &&
        renderTerminalSizeOk(&terminalWidth, &terminalHeight) &&
        terminalWidth >= MIN_TERMINAL_WIDTH &&
        terminalHeight >= MIN_TERMINAL_HEIGHT &&
        renderLayoutCurrent() == NULL &&
        canvasBegin()) {
        renderSplash(title, items, itemCount, disabledMask, selectedIndex);
        return;
    }
    renderMenuOverlay(title, items, itemCount, disabledMask, selectedIndex,
                      startRow);
}
