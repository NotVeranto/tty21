#ifndef RENDER_CANVAS_H
#define RENDER_CANVAS_H

/*
 * Off-screen canvas used by every full-screen renderer.
 *
 * All drawing goes into a grid of cells (one UTF-8 glyph + one style per
 * cell). canvasFlush() then emits the grid as a single buffered write:
 *   - rows that did not change since the previous flush are skipped,
 *     changed rows are re-sent only over the changed span,
 *   - the burst is wrapped in "synchronized output" (DEC mode 2026) so
 *     supporting terminals present it atomically; others simply ignore it,
 *   - line wrap is disabled while painting, so a glyph in the last column
 *     can never scroll the screen,
 *   - nothing ever clears the screen between frames, so nothing flickers.
 *
 * Coordinates are 1-based (column x, row y), like ANSI.
 */

#include "render_style.h"

typedef struct {
    /* double frame */
    const char *fTL, *fTR, *fBL, *fBR, *fH, *fV;
    /* single rule that joins the double frame, and its caps */
    const char *teeL, *teeR, *rule, *capL, *capR;
    /* rounded card border */
    const char *cTL, *cTR, *cBL, *cBR, *cH, *cV;
    /* J / Q / K crest frames: TL TR BL BR H V */
    const char *crest[3][6];
    const char *suit[4];
    const char *dot;
    const char *bullet;
    const char *dealer;
    const char *player;
    const char *pointer;
    const char *star;
    const char *sepV;
    const char *barFull;
    const char *barEmpty;
    const char *slash;
    const char *backslash;
    const char *chipL;
    const char *chipR;
    const char *chipFace;
    const char *chipDot;
    const char *arrowUp;
    const char *arrowDown;
    const char *check;
    const char *orn;
} Glyphs;

const Glyphs *renderGlyphs(void);

/* Returns 1 when the canvas can be used (terminal with cursor
 * addressing). Allocates / resizes it to the current terminal size. */
int canvasBegin(void);
int canvasWidth(void);
int canvasHeight(void);

/* Drawing. Anything outside the canvas is silently clipped. */
void canvasBlank(void);
void canvasFill(int x, int y, int w, int h, RenderStyle style);
void canvasPutGlyph(int x, int y, const char *glyph, RenderStyle style);
int canvasPutText(int x, int y, const char *utf8, RenderStyle style);
/* Writes at most maxCols glyphs of the text (no ellipsis). */
int canvasPutTextN(int x, int y, int maxCols, const char *utf8,
                   RenderStyle style);
/* Writes at most maxCols columns, ending in "..." when truncated. */
int canvasPutFit(int x, int y, int maxCols, const char *utf8,
                 RenderStyle style);
void canvasHLine(int x, int y, int w, const char *glyph, RenderStyle style);
void canvasVLine(int x, int y, int h, const char *glyph, RenderStyle style);
int canvasTextWidth(const char *utf8);
/* Centres text inside [x, x+w) and returns the column it starts at. */
int canvasPutCentered(int x, int w, int y, const char *utf8,
                      RenderStyle style);

/* Double-line box with an optional title set into the top border. */
void canvasBox(int x, int y, int w, int hgt, const char *title,
               RenderStyle frame, RenderStyle fill, RenderStyle titleStyle);

/* Marks a row to be re-sent in full on the next flush. Use it after
 * code outside the canvas has written to that row. */
void canvasTouchRow(int y);
/* Forget what is on screen: next flush repaints every row. */
void canvasInvalidate(void);
/* Forget what is on screen and clear the real screen on next flush. */
void canvasForceClear(void);

/* Emits the canvas. `extra` (optional) runs after the cells were written
 * but before the synchronized-update bracket closes; it may printf. */
void canvasFlushEx(void (*extra)(const void *ctx), const void *ctx);
void canvasFlush(void);

/* UTF-8 helpers shared by the renderers. */
int renderUtf8Length(const char *utf8);

#endif
