#include "render_cards.h"
#include "render_canvas.h"
#include "render_scene.h"
#include "render_style.h"
#include "round_cards.h"

#include <stdio.h>
#include <string.h>

#define COMPACT_CARD_W 7
#define COMPACT_CARD_H 5
#define FULL_CARD_H 7
#define MIN_CARDS_ACROSS 5

static const char *CARD_RANKS[] = {
    "", "A", "2", "3", "4", "5", "6",
    "7", "8", "9", "10", "J", "Q", "K"
};

static TableLayout currentLayout;
static int haveLayout;
static int hwFull;
static int hwCompact;
static int hwRound = -1;
static int hwTermW;

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/* ------------------------------------------------------------------ */

int getDynamicTableWidth(const Game *game) {
    int cardsTotal;
    int playerCardsTotal;
    int handIndex;
    int dealerHandIndex;
    int totalDealerCards;

    totalDealerCards = 0;
    for (dealerHandIndex = 0;
         dealerHandIndex < game->dealer.handCount;
         dealerHandIndex++) {
        totalDealerCards += game->dealer.hands[dealerHandIndex].count;
    }
    if (game->dealer.handCount > 1) {
        totalDealerCards += (game->dealer.handCount - 1) * 2;
    }

    cardsTotal = totalDealerCards;
    playerCardsTotal = 0;
    for (handIndex = 0;
         handIndex < game->player.handCount;
         handIndex++) {
        playerCardsTotal += game->player.hands[handIndex].count;
    }
    if (game->player.handCount > 1) {
        playerCardsTotal += (game->player.handCount - 1) * 2;
    }
    if (playerCardsTotal > cardsTotal) {
        cardsTotal = playerCardsTotal;
    }
    if (cardsTotal < 5) {
        cardsTotal = 5;
    }
    return cardsTotal * CARD_WIDTH + (cardsTotal - 1) * CARD_GAP;
}

static int clampCount(int count) {
    if (count < 0) return 0;
    if (count > RMAXC) return RMAXC;
    return count;
}

static int clampHands(int handCount) {
    if (handCount < 0) return 0;
    if (handCount > RMAXH) return RMAXH;
    return handCount;
}

static int sideNeed(const Hand *hands, int handCount, int cardW, int gap,
                    int handGap) {
    int total;
    int shown;
    int index;
    int count;

    total = 0;
    shown = 0;
    handCount = clampHands(handCount);
    for (index = 0; index < handCount; index++) {
        count = clampCount(hands[index].count);
        if (count <= 0) {
            continue;
        }
        total += count * cardW + (count - 1) * gap;
        shown++;
    }
    if (shown > 1) {
        total += (shown - 1) * handGap;
    }
    return total;
}

static void fillLayout(TableLayout *layout, int compact, int content,
                       int termW, int termH) {
    int framed;

    memset(layout, 0, sizeof(*layout));
    layout->termW = termW;
    layout->termH = termH;
    layout->compact = compact;
    layout->cardW = compact ? COMPACT_CARD_W : CARD_WIDTH;
    layout->cardH = compact ? COMPACT_CARD_H : FULL_CARD_H;
    layout->gap = CARD_GAP;
    layout->handGap = 3;
    layout->cardYOff = compact ? 1 : 0;
    layout->gutter = 0;
    if (!compact && content + 2 * RT_GUTTER + 2 <= termW) {
        layout->gutter = RT_GUTTER;
    }
    framed = layout->gutter > 0 ? content + 2 * layout->gutter + 2
                                : content + 4;
    layout->fw = framed;
    layout->fl = (termW - framed) / 2 + 1;
    if (layout->fl < 1) {
        layout->fl = 1;
    }
    layout->innerL = layout->fl + 1;
    layout->innerR = layout->fl + framed - 2;
    layout->cwid = content;
    layout->cx = layout->innerL + (layout->gutter > 0 ? layout->gutter : 1);
    layout->shoeX = layout->cx + content + 1;
    layout->shoeY = RT_ROW_DEALER_CARDS;
    layout->potX = layout->innerL + 1;
    layout->potY = RT_ROW_PLAYER_CARDS;
}

int renderLayoutCompute(const Game *game, int termW, int termH,
                        TableLayout *out) {
    int need;
    int dealerNeed;
    int playerNeed;
    int content;
    int available;
    int mode;
    int minContent;
    int cardW;
    int *highWater;

    if (game == NULL || out == NULL || termW < 8) {
        return 0;
    }
    if (game->roundNumber < hwRound || termW != hwTermW) {
        hwFull = 0;
        hwCompact = 0;
    }
    hwRound = game->roundNumber;
    hwTermW = termW;

    available = termW - 4;
    for (mode = 0; mode < 2; mode++) {
        cardW = mode == 0 ? CARD_WIDTH : COMPACT_CARD_W;
        minContent = MIN_CARDS_ACROSS * cardW + (MIN_CARDS_ACROSS - 1) *
                                                    CARD_GAP;
        highWater = mode == 0 ? &hwFull : &hwCompact;
        dealerNeed = sideNeed(game->dealer.hands, game->dealer.handCount,
                              cardW, CARD_GAP, 3);
        playerNeed = sideNeed(game->player.hands, game->player.handCount,
                              cardW, CARD_GAP, 3);
        need = dealerNeed > playerNeed ? dealerNeed : playerNeed;
        if (need < minContent) {
            need = minContent;
        }
        if (need > available) {
            continue;
        }
        /* The frame only ever grows during a run so it never jumps. */
        if (need > *highWater) {
            *highWater = need;
        }
        content = *highWater <= available ? *highWater : need;
        fillLayout(out, mode == 1, content, termW, termH);
        return 1;
    }
    return 0;
}

const TableLayout *renderLayoutCurrent(void) {
    return haveLayout ? &currentLayout : NULL;
}

void renderLayoutSetCurrent(const TableLayout *layout) {
    if (layout != NULL) {
        currentLayout = *layout;
        haveLayout = 1;
    }
}

void renderLayoutForget(void) {
    haveLayout = 0;
}

void renderHudRegion(int *x, int *width) {
    if (haveLayout) {
        *x = currentLayout.fl;
        *width = currentLayout.fw;
    } else {
        *x = 1;
        *width = canvasWidth();
    }
}

void renderLayoutHandsX(const TableLayout *layout, const int *counts,
                        int handCount, int x[RMAXH][RMAXC]) {
    int shown;
    int total;
    int index;
    int card;
    int count;
    int cursor;

    handCount = clampHands(handCount);
    shown = 0;
    total = 0;
    for (index = 0; index < handCount; index++) {
        count = clampCount(counts[index]);
        if (count > 0) {
            total += count * layout->cardW + (count - 1) * layout->gap;
            shown++;
        }
    }
    if (shown > 1) {
        total += (shown - 1) * layout->handGap;
    }
    cursor = layout->cx + (layout->cwid - total) / 2;
    if (cursor < layout->innerL) {
        cursor = layout->innerL;
    }
    for (index = 0; index < handCount; index++) {
        count = clampCount(counts[index]);
        for (card = 0; card < RMAXC; card++) {
            x[index][card] = cursor + card * (layout->cardW + layout->gap);
        }
        if (count > 0) {
            cursor += count * layout->cardW + (count - 1) * layout->gap +
                      layout->handGap;
        }
    }
}

void renderSceneFromGame(const Game *game, Scene *scene) {
    int index;

    memset(scene, 0, sizeof(*scene));
    for (index = 0; index < RMAXH; index++) {
        scene->from[SIDE_DEALER][index] =
            index < game->dealer.handCount
                ? clampCount(game->dealer.hands[index].count) : 0;
        scene->from[SIDE_PLAYER][index] =
            index < game->player.handCount
                ? clampCount(game->player.hands[index].count) : 0;
    }
    memcpy(scene->to, scene->from, sizeof(scene->to));
    scene->t256 = 256;
    scene->chipsShown = -1;
    scene->deckLeft = -1;
}

/* ------------------------------------------------------------------ */
/* Card faces                                                          */
/* ------------------------------------------------------------------ */

/* Pip layouts for ranks 1-10 on a 5-row grid. bit0/bit1/bit2 select the
 * left / centre / right pip column. */
static const unsigned char PIPS[11][5] = {
    { 0, 0, 0, 0, 0 },
    { 0, 0, 2, 0, 0 },
    { 2, 0, 0, 0, 2 },
    { 2, 0, 2, 0, 2 },
    { 5, 0, 0, 0, 5 },
    { 5, 0, 2, 0, 5 },
    { 5, 0, 5, 0, 5 },
    { 5, 2, 5, 0, 5 },
    { 5, 2, 5, 2, 5 },
    { 5, 5, 2, 5, 5 },
    { 5, 7, 0, 7, 5 },
};

static const char *rankName(int rank) {
    return rank >= 1 && rank <= 13 ? CARD_RANKS[rank] : "?";
}

static const char *suitGlyph(const Glyphs *glyphs, int suit) {
    return suit >= 0 && suit < 4 ? glyphs->suit[suit] : "?";
}

static RenderStyle inkStyle(const Card *card) {
    return card->suit == HEARTS || card->suit == DIAMONDS ? ST_CARD_RED
                                                           : ST_CARD_BLACK;
}

static RenderStyle curseStyle(int cursed) {
    switch (cursed) {
        case CURSED_BLEED: return ST_CARD_BLEED;
        case CURSED_FROST: return ST_CARD_FROST;
        case CURSED_ECHO:  return ST_CARD_ECHO;
        default: return ST_CARD_BLACK;
    }
}

static const char *curseTag(int cursed) {
    switch (cursed) {
        case CURSED_BLEED: return "*B";
        case CURSED_FROST: return "*F";
        case CURSED_ECHO:  return "*E";
        default: return "*?";
    }
}

static void drawBox(const Glyphs *glyphs, int x, int y, int w, int h,
                    RenderStyle style) {
    int row;

    canvasPutGlyph(x, y, glyphs->cTL, style);
    canvasHLine(x + 1, y, w - 2, glyphs->cH, style);
    canvasPutGlyph(x + w - 1, y, glyphs->cTR, style);
    for (row = 1; row < h - 1; row++) {
        canvasPutGlyph(x, y + row, glyphs->cV, style);
        canvasPutGlyph(x + w - 1, y + row, glyphs->cV, style);
    }
    canvasPutGlyph(x, y + h - 1, glyphs->cBL, style);
    canvasHLine(x + 1, y + h - 1, w - 2, glyphs->cH, style);
    canvasPutGlyph(x + w - 1, y + h - 1, glyphs->cBR, style);
}

/* Lattice used on card backs: interior is w x h cells. */
static void drawLattice(const Glyphs *glyphs, int x, int y, int w, int h) {
    int row;
    int col;

    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            canvasPutGlyph(x + col, y + row,
                           ((row + col) & 1) ? glyphs->backslash
                                             : glyphs->slash,
                           ST_CARD_BACK);
        }
    }
}

/* 9x5 card back box with the TTY21 crest. `count` >= 0 prints the shoe
 * size in the middle instead of the crest. */
static void drawBackBox(const Glyphs *glyphs, int x, int y, int w, int h,
                        int count) {
    char number[16];
    int textW;

    drawBox(glyphs, x, y, w, h, ST_CARD_BACK_EDGE);
    drawLattice(glyphs, x + 1, y + 1, w - 2, h - 2);
    if (count >= 0) {
        snprintf(number, sizeof(number), "%d", count);
        textW = (int)strlen(number);
        canvasFill(x + 1, y + h / 2, w - 2, 1, ST_CARD_BACK_BRAND);
        if (textW <= w - 2) {
            canvasPutText(x + 1 + (w - 2 - textW) / 2, y + h / 2, number,
                          ST_CARD_BACK_BRAND);
        }
        return;
    }
    if (w >= 9 && h >= 5) {
        int half;
        int mid;
        int top;

        half = w >= 11 ? 3 : 2;
        mid = x + w / 2;
        top = y + h / 2 - 1;
        canvasPutGlyph(mid - half, top, glyphs->cTL, ST_CARD_BACK_BRAND);
        canvasHLine(mid - half + 1, top, 2 * half - 1, glyphs->cH,
                    ST_CARD_BACK_BRAND);
        canvasPutGlyph(mid + half, top, glyphs->cTR, ST_CARD_BACK_BRAND);
        canvasPutGlyph(mid - half, top + 1, glyphs->cV, ST_CARD_BACK_BRAND);
        canvasFill(mid - half + 1, top + 1, 2 * half - 1, 1,
                   ST_CARD_BACK_BRAND);
        if (half == 3) {
            canvasPutText(mid - 2, top + 1, "TTY21", ST_CARD_BACK_BRAND);
        } else {
            canvasPutText(mid - 1, top + 1, "21", ST_CARD_BACK_BRAND);
            canvasPutGlyph(mid + 1, top + 1, glyphs->suit[3],
                           ST_CARD_BACK_BRAND);
        }
        canvasPutGlyph(mid + half, top + 1, glyphs->cV, ST_CARD_BACK_BRAND);
        canvasPutGlyph(mid - half, top + 2, glyphs->cBL, ST_CARD_BACK_BRAND);
        canvasHLine(mid - half + 1, top + 2, 2 * half - 1, glyphs->cH,
                    ST_CARD_BACK_BRAND);
        canvasPutGlyph(mid + half, top + 2, glyphs->cBR, ST_CARD_BACK_BRAND);
    } else {
        canvasPutGlyph(x + w / 2, y + h / 2, glyphs->suit[3],
                       ST_CARD_BACK_BRAND);
    }
}

static void drawFaceFull(const Glyphs *glyphs, const Card *card, int x,
                         int y, RenderStyle edge) {
    RenderStyle ink;
    const char *rank;
    const char *suit;
    int rankW;
    int row;
    int col;
    int rankIndex;
    static const int PIP_COLS[3] = { 2, 4, 6 };

    ink = inkStyle(card);
    rank = rankName(card->rank);
    suit = suitGlyph(glyphs, card->suit);
    rankW = (int)strlen(rank);

    drawBox(glyphs, x, y, 11, 7, edge);
    canvasFill(x + 1, y + 1, 9, 5, ink);

    canvasPutText(x + 1, y + 1, rank, ink);
    canvasPutText(x + 10 - rankW, y + 5, rank, ink);
    if (card->cursed != CURSED_NONE) {
        canvasPutText(x + 8, y + 1, curseTag(card->cursed),
                      curseStyle(card->cursed));
    }

    rankIndex = card->rank;
    if (rankIndex >= 1 && rankIndex <= 10) {
        for (row = 0; row < 5; row++) {
            for (col = 0; col < 3; col++) {
                if ((PIPS[rankIndex][row] >> col) & 1) {
                    canvasPutGlyph(x + 1 + PIP_COLS[col], y + 1 + row, suit,
                                   ink);
                }
            }
        }
    } else if (rankIndex >= 11 && rankIndex <= 13) {
        const char *const *crest = glyphs->crest[rankIndex - 11];

        canvasPutGlyph(x + 2, y + 2, crest[0], ink);
        canvasHLine(x + 3, y + 2, 5, crest[4], ink);
        canvasPutGlyph(x + 8, y + 2, crest[1], ink);
        canvasPutGlyph(x + 2, y + 3, crest[5], ink);
        canvasPutGlyph(x + 3, y + 3, suit, ink);
        canvasPutText(x + 5, y + 3, rank, ink);
        canvasPutGlyph(x + 7, y + 3, suit, ink);
        canvasPutGlyph(x + 8, y + 3, crest[5], ink);
        canvasPutGlyph(x + 2, y + 4, crest[2], ink);
        canvasHLine(x + 3, y + 4, 5, crest[4], ink);
        canvasPutGlyph(x + 8, y + 4, crest[3], ink);
    } else {
        canvasPutGlyph(x + 5, y + 3, suit, ink);
    }
}

static void drawFaceCompact(const Glyphs *glyphs, const Card *card, int x,
                            int y, RenderStyle edge) {
    RenderStyle ink;
    const char *rank;

    ink = inkStyle(card);
    rank = rankName(card->rank);
    drawBox(glyphs, x, y, 7, 5, edge);
    canvasFill(x + 1, y + 1, 5, 3, ink);
    canvasPutText(x + 1, y + 1, rank, ink);
    canvasPutGlyph(x + 3, y + 2, suitGlyph(glyphs, card->suit), ink);
    canvasPutText(x + 6 - (int)strlen(rank), y + 3, rank, ink);
    if (card->cursed != CURSED_NONE) {
        canvasPutGlyph(x + 5, y + 1, "*", curseStyle(card->cursed));
    }
}

/* A card seen edge-on while it turns over. */
static void drawNarrow(const TableLayout *layout, const Glyphs *glyphs,
                       const Card *card, int faceDown, int x, int y,
                       int scaleW, RenderStyle edge) {
    RenderStyle edgeStyle;
    RenderStyle body;
    int row;
    int col;

    x += (layout->cardW - scaleW) / 2;
    edgeStyle = faceDown ? ST_CARD_BACK_EDGE : edge;
    body = faceDown ? ST_CARD_BACK : inkStyle(card);
    for (row = 0; row < layout->cardH; row++) {
        if (scaleW == 1) {
            canvasPutGlyph(x, y + row, glyphs->cV, edgeStyle);
            continue;
        }
        if (row == 0) {
            canvasPutGlyph(x, y, glyphs->cTL, edgeStyle);
            canvasHLine(x + 1, y, scaleW - 2, glyphs->cH, edgeStyle);
            canvasPutGlyph(x + scaleW - 1, y, glyphs->cTR, edgeStyle);
        } else if (row == layout->cardH - 1) {
            canvasPutGlyph(x, y + row, glyphs->cBL, edgeStyle);
            canvasHLine(x + 1, y + row, scaleW - 2, glyphs->cH, edgeStyle);
            canvasPutGlyph(x + scaleW - 1, y + row, glyphs->cBR, edgeStyle);
        } else {
            canvasPutGlyph(x, y + row, glyphs->cV, edgeStyle);
            for (col = 1; col < scaleW - 1; col++) {
                if (faceDown) {
                    canvasPutGlyph(x + col, y + row,
                                   ((row + col) & 1) ? glyphs->backslash
                                                     : glyphs->slash,
                                   body);
                } else {
                    canvasPutGlyph(x + col, y + row, " ", body);
                }
            }
            canvasPutGlyph(x + scaleW - 1, y + row, glyphs->cV, edgeStyle);
        }
    }
    if (!faceDown && scaleW >= 5) {
        canvasPutGlyph(x + scaleW / 2, y + layout->cardH / 2,
                       suitGlyph(glyphs, card->suit), body);
    }
}

void renderCardSprite(const TableLayout *layout, const Card *card,
                      int faceDown, int x, int y, int scaleW,
                      RenderStyle edge) {
    const Glyphs *glyphs;

    glyphs = renderGlyphs();
    if (scaleW > 0 && scaleW < layout->cardW) {
        drawNarrow(layout, glyphs, card, faceDown, x, y, scaleW, edge);
        return;
    }
    if (faceDown) {
        drawBackBox(glyphs, x, y, layout->cardW, layout->cardH, -1);
        return;
    }
    if (layout->compact) {
        drawFaceCompact(glyphs, card, x, y, edge);
    } else {
        drawFaceFull(glyphs, card, x, y, edge);
    }
}

/* ------------------------------------------------------------------ */
/* Hands                                                               */
/* ------------------------------------------------------------------ */

static int lerp(int from, int to, int t256) {
    return from + ((to - from) * t256) / 256;
}

void renderCardsDrawSide(const Game *game, int side,
                         const TableLayout *layout, const Scene *scene,
                         int hideFirst) {
    const Hand *hands;
    int handCount;
    int xFrom[RMAXH][RMAXC];
    int xTo[RMAXH][RMAXC];
    int handIndex;
    int cardIndex;
    int count;
    int x;
    int y;
    int faceDown;
    int scaleW;
    int activeMark;
    RenderStyle edge;

    hands = side == SIDE_DEALER ? game->dealer.hands : game->player.hands;
    handCount = clampHands(side == SIDE_DEALER ? game->dealer.handCount
                                               : game->player.handCount);
    y = (side == SIDE_DEALER ? RT_ROW_DEALER_CARDS : RT_ROW_PLAYER_CARDS) +
        layout->cardYOff;

    renderLayoutHandsX(layout, scene->from[side], handCount, xFrom);
    renderLayoutHandsX(layout, scene->to[side], handCount, xTo);

    for (handIndex = 0; handIndex < handCount; handIndex++) {
        count = clampCount(hands[handIndex].count);
        activeMark = side == SIDE_PLAYER && game->player.handCount > 1 &&
                     handIndex == game->player.activeHand &&
                     game->state == STATE_PLAYER_TURN;
        edge = activeMark ? ST_CARD_EDGE_ACTIVE : ST_CARD_EDGE;
        for (cardIndex = 0; cardIndex < count; cardIndex++) {
            x = lerp(xFrom[handIndex][cardIndex],
                     xTo[handIndex][cardIndex], scene->t256);
            faceDown = side == SIDE_DEALER && hideFirst && cardIndex == 0;
            scaleW = 0;
            if (scene->flip && side == SIDE_DEALER && handIndex == 0 &&
                cardIndex == 0) {
                faceDown = !scene->flipFaceUp;
                scaleW = scene->flipW;
            }
            renderCardSprite(layout, &hands[handIndex].cards[cardIndex],
                             faceDown, x, y, scaleW, edge);
        }
    }
}

void renderCardsDrawBadges(const Game *game, const TableLayout *layout,
                           const Scene *scene) {
    const Glyphs *glyphs;
    int xFrom[RMAXH][RMAXC];
    int xTo[RMAXH][RMAXC];
    int handIndex;
    int handCount;
    int count;
    int value;
    int width;
    int start;
    int active;
    char text[48];
    char status[24];
    RenderStyle style;

    glyphs = renderGlyphs();
    handCount = clampHands(game->player.handCount);
    renderLayoutHandsX(layout, scene->from[SIDE_PLAYER], handCount, xFrom);
    renderLayoutHandsX(layout, scene->to[SIDE_PLAYER], handCount, xTo);

    for (handIndex = 0; handIndex < handCount; handIndex++) {
        count = clampCount(game->player.hands[handIndex].count);
        if (count <= 0) {
            continue;
        }
        value = calculatePlayerHandValueForIndex(game, handIndex);
        active = handIndex == game->player.activeHand &&
                 game->player.activeHand < game->player.handCount;

        if (value > 21) {
            snprintf(status, sizeof(status), "BUST");
            style = ST_BADGE_RED;
        } else if (count == 2 && value == 21) {
            snprintf(status, sizeof(status), "BLACKJACK");
            style = ST_BADGE_OK;
        } else {
            snprintf(status, sizeof(status), "%d", value);
            style = active ? ST_BADGE_GOLD : ST_FELT_MUTED;
        }
        if (game->player.handCount > 1) {
            snprintf(text, sizeof(text), " %s HAND %d  %s %s ",
                     active ? glyphs->pointer : " ", handIndex + 1,
                     glyphs->sepV, status);
        } else {
            snprintf(text, sizeof(text), " %s ", status);
        }

        width = count * layout->cardW + (count - 1) * layout->gap;
        start = lerp(xFrom[handIndex][0], xTo[handIndex][0], scene->t256);
        start += (width - (int)strlen(text)) / 2;
        canvasPutText(start, RT_ROW_BADGES, text, style);
    }
}

void renderDrawDealerHands(const Game *game, int padding,
                           int hideFirstCard) {
    const TableLayout *layout;
    Scene scene;

    (void)padding;
    layout = renderLayoutCurrent();
    if (layout == NULL || !canvasBegin()) {
        return;
    }
    renderSceneFromGame(game, &scene);
    renderCardsDrawSide(game, SIDE_DEALER, layout, &scene, hideFirstCard);
}

void renderDrawPlayerHands(const Game *game, int padding) {
    const TableLayout *layout;
    Scene scene;

    (void)padding;
    layout = renderLayoutCurrent();
    if (layout == NULL || !canvasBegin()) {
        return;
    }
    renderSceneFromGame(game, &scene);
    renderCardsDrawSide(game, SIDE_PLAYER, layout, &scene, 0);
    renderCardsDrawBadges(game, layout, &scene);
}

/* ------------------------------------------------------------------ */
/* Shoe and bet pot                                                    */
/* ------------------------------------------------------------------ */

void renderShoe(const TableLayout *layout, int cardsLeft) {
    const Glyphs *glyphs;
    int layers;
    int layer;
    int x;
    int y;
    int row;
    int col;

    glyphs = renderGlyphs();
    x = layout->shoeX;
    y = layout->shoeY;

    if (cardsLeft <= 0) {
        drawBox(glyphs, x, y, 9, 5, ST_SLOT);
        for (row = 1; row < 4; row++) {
            for (col = 1; col < 8; col++) {
                canvasPutGlyph(x + col, y + row, glyphs->dot, ST_SLOT);
            }
        }
        canvasPutText(x + 2, y + 2, "EMPTY", ST_SLOT);
        return;
    }
    layers = cardsLeft >= 35 ? 3 : (cardsLeft >= 12 ? 2 : 1);
    for (layer = layers - 1; layer >= 0; layer--) {
        drawBackBox(glyphs, x + layer, y + layer, 9, 5,
                    layer == 0 ? cardsLeft : -1);
    }
}

void renderChipPot(const TableLayout *layout, int bet, int minBet) {
    const Glyphs *glyphs;
    char label[32];
    int ratio;
    int chips;
    int index;
    int x;
    int baseRow;
    int offset;
    RenderStyle style;

    glyphs = renderGlyphs();
    if (minBet < 1) {
        minBet = 1;
    }
    baseRow = layout->potY + 5;
    if (bet > 0) {
        ratio = bet / minBet;
        chips = 1 + (ratio >= 2) + (ratio >= 4) + (ratio >= 8) +
                (ratio >= 16);
        if (ratio < 2) style = ST_CHIP_RED;
        else if (ratio < 5) style = ST_CHIP_BLUE;
        else if (ratio < 10) style = ST_CHIP_GREEN;
        else if (ratio < 25) style = ST_CHIP_GOLD;
        else style = ST_CHIP_BLACK;
        for (index = 0; index < chips; index++) {
            offset = 2 + (index & 1);
            x = layout->potX + offset;
            canvasPutGlyph(x, baseRow - index, glyphs->chipL, style);
            canvasPutGlyph(x + 1, baseRow - index, glyphs->chipFace, style);
            canvasPutGlyph(x + 2, baseRow - index, " ", style);
            canvasPutGlyph(x + 3, baseRow - index, glyphs->chipDot, style);
            canvasPutGlyph(x + 4, baseRow - index, " ", style);
            canvasPutGlyph(x + 5, baseRow - index, glyphs->chipFace, style);
            canvasPutGlyph(x + 6, baseRow - index, glyphs->chipR, style);
        }
        snprintf(label, sizeof(label), "BET %d", bet);
        canvasPutCentered(layout->potX, 11, layout->potY + 6, label,
                          ST_FELT_GOLD);
    } else {
        canvasPutCentered(layout->potX, 11, baseRow, "( )", ST_FELT);
        canvasPutCentered(layout->potX, 11, layout->potY + 6, "BET",
                          ST_FELT_MUTED);
    }
}
