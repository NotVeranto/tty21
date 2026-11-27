#define _DEFAULT_SOURCE
#include "render_status.h"
#include "render_canvas.h"
#include "render_rows.h"
#include "render_scene.h"
#include "render_style.h"
#include "round_cards.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MAX_SEGMENTS 8

typedef struct {
    char text[72];
    RenderStyle style;
    int priority;
    int floorBar;     /* draw the floor progress bar after the text */
    int noSeparator;  /* glued to the previous segment */
    int width;
} Segment;

static int activeHandIndex(const Game *game) {
    int index;

    index = game->player.activeHand;
    if (index >= game->player.handCount) {
        index = game->player.handCount - 1;
    }
    if (index < 0) {
        index = 0;
    }
    return index;
}

static int segmentWidth(const Segment *segment) {
    return canvasTextWidth(segment->text) + (segment->floorBar ? 11 : 0);
}

static int totalWidth(const Segment *segments, int count) {
    int total;
    int index;

    total = 0;
    for (index = 0; index < count; index++) {
        total += segments[index].width;
        if (index > 0 && !segments[index].noSeparator) {
            total += 3;
        }
    }
    return total;
}

static void removeSegment(Segment *segments, int *count, int at) {
    int index;

    /* A glued delta goes away together with its owner. */
    if (at + 1 < *count && segments[at + 1].noSeparator) {
        for (index = at + 1; index + 1 < *count; index++) {
            segments[index] = segments[index + 1];
        }
        (*count)--;
    }
    for (index = at; index + 1 < *count; index++) {
        segments[index] = segments[index + 1];
    }
    (*count)--;
}

static int addSegment(Segment *segments, int count, RenderStyle style,
                      int priority, const char *format, ...) {
    va_list args;

    if (count >= MAX_SEGMENTS) {
        return count;
    }
    memset(&segments[count], 0, sizeof(segments[count]));
    va_start(args, format);
    vsnprintf(segments[count].text, sizeof(segments[count].text), format,
              args);
    va_end(args);
    segments[count].style = style;
    segments[count].priority = priority;
    return count + 1;
}

static int buildSegments(const Game *game, int displayedChips,
                         int chipDelta, int showDeck, int shortForm,
                         Segment *segments) {
    int count;
    int hand;
    int cardsLeft;
    int index;
    char betText[48];

    count = 0;
    hand = activeHandIndex(game);
    cardsLeft = DECK_SIZE - game->deck.top;
    if (cardsLeft < 0) {
        cardsLeft = 0;
    }

    if (game->mode == MODE_ROGUELIKE) {
        count = addSegment(segments, count, ST_STRONG, 8,
                           shortForm ? "F%d" : "FLOOR %d", game->run.floor);
        segments[count - 1].floorBar = !shortForm;
        count = addSegment(segments, count, ST_ACCENT, 6,
                           shortForm ? "M%d" : "MARKS %d", game->run.marks);
    }

    count = addSegment(segments, count, ST_GOLD_B, 10,
                       shortForm ? "C%d" : "CHIPS %d", displayedChips);
    if (chipDelta != 0) {
        count = addSegment(segments, count, chipDelta > 0 ? ST_OK : ST_BAD,
                           10, "%+d", chipDelta);
        segments[count - 1].noSeparator = 1;
    }

    if (game->player.handCount > 1) {
        snprintf(betText, sizeof(betText),
                 shortForm ? "B%d h%d/%d" : "BET %d (hand %d/%d)",
                 game->player.bets[hand], hand + 1, game->player.handCount);
    } else {
        snprintf(betText, sizeof(betText), shortForm ? "B%d" : "BET %d",
                 game->player.bets[hand]);
    }
    count = addSegment(segments, count, ST_STRONG, 9, "%s", betText);

    if (game->mode == MODE_ROGUELIKE && game->loanAmount > 0) {
        count = addSegment(segments, count, ST_BAD, 5,
                           shortForm ? "L%d>F%d" : "LOAN %d by F%d",
                           game->loanAmount, game->loanDueFloor);
    }
    if (showDeck) {
        count = addSegment(segments, count, ST_MUTED, 3,
                           shortForm ? "D%d" : "DECK %d", cardsLeft);
    }
    for (index = 0; index < count; index++) {
        segments[index].width = segmentWidth(&segments[index]);
    }
    return count;
}

static void dropLeastImportant(Segment *segments, int *count) {
    int index;
    int worst;

    worst = -1;
    for (index = 0; index < *count; index++) {
        if (segments[index].noSeparator) {
            continue;
        }
        if (worst < 0 || segments[index].priority <
                             segments[worst].priority) {
            worst = index;
        }
    }
    if (worst >= 0) {
        removeSegment(segments, count, worst);
    }
}

void renderStatusRow(const Game *game, int displayedChips, int chipDelta,
                     int showDeck) {
    const Glyphs *glyphs;
    Segment segments[MAX_SEGMENTS];
    int count;
    int x0;
    int width;
    int x;
    int index;
    int progress;
    int bar;

    glyphs = renderGlyphs();
    renderHudRegion(&x0, &width);
    canvasFill(x0, ROW_STATUS, width, 1, ST_PLAIN);
    canvasTouchRow(ROW_STATUS);

    count = buildSegments(game, displayedChips, chipDelta, showDeck, 0,
                          segments);
    if (totalWidth(segments, count) > width) {
        count = buildSegments(game, displayedChips, chipDelta, showDeck, 1,
                              segments);
    }
    while (count > 1 && totalWidth(segments, count) > width) {
        dropLeastImportant(segments, &count);
    }

    progress = game->run.floor % 10;
    if (progress < 0) {
        progress = 0;
    }

    x = x0;
    for (index = 0; index < count; index++) {
        if (index > 0 && !segments[index].noSeparator) {
            canvasPutText(x, ROW_STATUS, " ", ST_PLAIN);
            canvasPutGlyph(x + 1, ROW_STATUS, glyphs->sepV, ST_BRASS);
            x += 3;
        } else if (index > 0) {
            x += 1;
        }
        x += canvasPutFit(x, ROW_STATUS, x0 + width - x,
                          segments[index].text, segments[index].style);
        if (segments[index].floorBar) {
            canvasPutText(x, ROW_STATUS, " ", ST_PLAIN);
            for (bar = 0; bar < 10; bar++) {
                canvasPutGlyph(x + 1 + bar, ROW_STATUS,
                               bar < progress ? glyphs->barFull
                                              : glyphs->barEmpty,
                               bar < progress ? ST_GOLD : ST_MUTED);
            }
            x += 11;
        }
    }
}

static void formatPlainStatus(const Game *game, char *buffer, size_t size) {
    int hand;
    int cardsLeft;
    int length;

    hand = activeHandIndex(game);
    cardsLeft = DECK_SIZE - game->deck.top;
    length = 0;
    if (game->mode == MODE_ROGUELIKE) {
        length = snprintf(buffer, size, "Floor: %d | Marks: %d | ",
                          game->run.floor, game->run.marks);
    } else {
        buffer[0] = '\0';
    }
    length += snprintf(buffer + length, size - (size_t)length,
                       "Chips: %d | Bet: %d", game->player.chips,
                       game->player.bets[hand]);
    if (game->player.handCount > 1) {
        length += snprintf(buffer + length, size - (size_t)length,
                           " (hand %d/%d)", hand + 1,
                           game->player.handCount);
    }
    length += snprintf(buffer + length, size - (size_t)length,
                       " | Deck: %d cards", cardsLeft);
    if (game->mode == MODE_ROGUELIKE && game->loanAmount > 0) {
        snprintf(buffer + length, size - (size_t)length,
                 "  Loan: %d by F%d", game->loanAmount, game->loanDueFloor);
    }
}

void renderStatusLine(const Game *game, int terminalWidth, int padding) {
    char text[256];

    (void)terminalWidth;
    (void)padding;
    if (canvasBegin()) {
        renderStatusRow(game, game->player.chips, 0, 1);
        canvasFlush();
        return;
    }
    formatPlainStatus(game, text, sizeof(text));
    printf("%s\n", text);
}
