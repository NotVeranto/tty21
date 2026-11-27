#include "render_log.h"
#include "render_canvas.h"
#include "render_rows.h"
#include "render_scene.h"
#include "render_style.h"

#include <string.h>

static RenderStyle logStyle(int style) {
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

/* Draws the event log, oldest line on top, older lines fading out. */
void renderLogRows(const Game *game, int padding, int terminalWidth) {
    const Glyphs *glyphs;
    RenderStyle style;
    int x0;
    int width;
    int rowIndex;
    int sourceIndex;
    int newest;

    (void)padding;
    (void)terminalWidth;
    glyphs = renderGlyphs();
    renderHudRegion(&x0, &width);

    for (rowIndex = 0; rowIndex < LOG_SIZE; rowIndex++) {
        canvasFill(x0, ROW_LOG_0 + rowIndex, width, 1, ST_PLAIN);
        canvasTouchRow(ROW_LOG_0 + rowIndex);
    }
    if (game->log.count <= 0) {
        return;
    }
    for (rowIndex = 0; rowIndex < LOG_SIZE; rowIndex++) {
        sourceIndex = rowIndex - (LOG_SIZE - game->log.count);
        if (sourceIndex < 0) {
            continue;
        }
        newest = sourceIndex == game->log.count - 1;
        style = logStyle(game->log.styles[sourceIndex]);
        if (!newest && sourceIndex < game->log.count - 2) {
            style |= RSTYLE_DIM;
        }
        canvasPutGlyph(x0, ROW_LOG_0 + rowIndex, glyphs->bullet,
                       newest ? ST_GOLD_B : ST_BRASS);
        canvasPutFit(x0 + 2, ROW_LOG_0 + rowIndex, width - 2,
                     game->log.lines[sourceIndex], style);
    }
}

void renderEventLog(const Game *game, int padding, int terminalWidth) {
    if (!canvasBegin()) {
        return;
    }
    renderLogRows(game, padding, terminalWidth);
    canvasFlush();
}
