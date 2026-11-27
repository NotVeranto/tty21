#define _DEFAULT_SOURCE
#include "render.h"
#include "render_style.h"
#include "render_layout.h"
#include "render_log.h"
#include "render_messages.h"
#include "render_cards.h"
#include "render_table_info.h"
#include "render_terminal.h"
#include "render_canvas.h"
#include "render_scene.h"
#include "render_rows.h"
#include "render_oracle.h"
#include "render_control.h"
#include "terminal.h"

#include "dealers.h"
#include "round_cards.h"
#include "round_rules.h"
#include "round_economy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void renderGameStateFallback(const Game *game, int hideDealerCard);

static int tablePadding;

/* What the previous table frame showed. Animations are derived from the
 * difference to the new state, so the game logic never has to call them. */
typedef struct {
    int valid;
    int round;
    int handCount[2];
    int count[2][RMAXH];
    int total[2];
    int dealerHidden;
    Card hole;
    int chips;
} FrameMemory;

static FrameMemory memory;

/* Pauses visible round presentation without delaying fast or non-interactive runs. */
void renderPause(const Game *game, int ms) {
    if (game == NULL || game->fastMode || ms <= 0 ||
        !isatty(STDOUT_FILENO)) {
        return;
    }

    usleep((unsigned int)ms * 1000U);
}

void renderTableMessage(const char *message,
                        int row,
                        MessageStyle style) {
    renderMessage(message, row, style, tablePadding);
}

void renderResetScreen(void) {
    renderInitColors();
    if (!isatty(STDOUT_FILENO)) {
        return;
    }
    printf("%s%s", SCR_CLEAR, SCR_HOME);
    renderShowCursor();
    tablePadding = 0;
    if (canvasBegin()) {
        canvasBlank();
        canvasInvalidate();
    }
    renderLayoutForget();
    renderMenuForgetSplash();
    fflush(stdout);
}

/* ------------------------------------------------------------------ */
/* Table composition                                                   */
/* ------------------------------------------------------------------ */

static const Card BLANK_CARD;

static int handIndexToShow(const Game *game) {
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

static int dealerHidden(const Game *game, int hideDealerCard) {
    return hideDealerCard &&
           !game->peekActive &&
           !twistActive(game, TWIST_SNIPER);
}

static unsigned int scatter(int x, int y) {
    return ((unsigned int)x * 73856093u) ^ ((unsigned int)y * 19349663u);
}

static void drawFelt(const TableLayout *layout) {
    const Glyphs *glyphs;
    int width;
    int x;
    int y;

    glyphs = renderGlyphs();
    width = layout->fw - 2;
    canvasFill(layout->innerL, RT_ROW_DEALER_RULE + 1, width,
               RT_ROW_BOTTOM - RT_ROW_DEALER_RULE - 1, ST_FELT_TEXT);
    if (!renderUtf8Enabled()) {
        return;
    }
    for (y = RT_ROW_DEALER_CARDS; y < RT_ROW_BADGES; y++) {
        if (y == RT_ROW_PLAYER_RULE) {
            continue;
        }
        for (x = layout->innerL; x <= layout->innerR; x++) {
            if (scatter(x, y) % 11u == 0u) {
                canvasPutGlyph(x, y, glyphs->dot, ST_FELT);
            }
        }
    }
}

static void drawFrame(const TableLayout *layout) {
    const Glyphs *glyphs;
    char title[64];
    int row;
    int right;
    int titleW;
    int titleX;
    int index;
    int x;

    glyphs = renderGlyphs();
    right = layout->fl + layout->fw - 1;

    canvasPutGlyph(layout->fl, RT_ROW_TOP, glyphs->fTL, ST_BRASS);
    canvasHLine(layout->fl + 1, RT_ROW_TOP, layout->fw - 2, glyphs->fH,
                ST_BRASS);
    canvasPutGlyph(right, RT_ROW_TOP, glyphs->fTR, ST_BRASS);

    if (renderUtf8Enabled()) {
        snprintf(title, sizeof(title), " %s T T Y 2 1 %s ", glyphs->suit[3],
                 glyphs->suit[0]);
    } else {
        snprintf(title, sizeof(title), " T T Y 2 1 ");
    }
    titleW = canvasTextWidth(title);
    titleX = layout->fl + (layout->fw - titleW) / 2;
    canvasPutGlyph(titleX - 1, RT_ROW_TOP, glyphs->capL, ST_BRASS);
    canvasPutText(titleX, RT_ROW_TOP, title, ST_GOLD_B);
    canvasPutGlyph(titleX + titleW, RT_ROW_TOP, glyphs->capR, ST_BRASS);

    for (row = RT_ROW_INFO; row < RT_ROW_BOTTOM; row++) {
        if (row == RT_ROW_DEALER_RULE || row == RT_ROW_PLAYER_RULE) {
            canvasPutGlyph(layout->fl, row, glyphs->teeL, ST_BRASS);
            canvasPutGlyph(right, row, glyphs->teeR, ST_BRASS);
        } else {
            canvasPutGlyph(layout->fl, row, glyphs->fV, ST_BRASS);
            canvasPutGlyph(right, row, glyphs->fV, ST_BRASS);
        }
    }

    canvasPutGlyph(layout->fl, RT_ROW_BOTTOM, glyphs->fBL, ST_BRASS);
    canvasHLine(layout->fl + 1, RT_ROW_BOTTOM, layout->fw - 2, glyphs->fH,
                ST_BRASS);
    canvasPutGlyph(right, RT_ROW_BOTTOM, glyphs->fBR, ST_BRASS);
    if (renderUtf8Enabled()) {
        static const int ORDER[4] = { 3, 0, 2, 1 };

        x = layout->fl + (layout->fw - 9) / 2;
        canvasPutGlyph(x - 1, RT_ROW_BOTTOM, glyphs->capL, ST_BRASS);
        canvasFill(x, RT_ROW_BOTTOM, 9, 1, ST_BRASS);
        for (index = 0; index < 4; index++) {
            canvasPutGlyph(x + 1 + index * 2, RT_ROW_BOTTOM,
                           glyphs->suit[ORDER[index]], ST_BRASS);
        }
        canvasPutGlyph(x + 9, RT_ROW_BOTTOM, glyphs->capR, ST_BRASS);
    }
}

static const char *phaseName(const Game *game) {
    switch (game->state) {
        case STATE_BET:         return "BET";
        case STATE_PLAYER_TURN: return "PLAYER TURN";
        case STATE_DEALER_TURN: return "DEALER TURN";
        case STATE_RESULT:      return "RESULT";
        default:                return "";
    }
}

static void drawInfoBar(const Game *game, const TableLayout *layout) {
    char text[48];
    const char *phase;
    const char *mode;
    int width;
    int x;
    int boss;

    width = layout->fw - 2;
    canvasFill(layout->innerL, RT_ROW_INFO, width, 1, ST_BAR);

    x = layout->innerL + 1;
    x += canvasPutText(x, RT_ROW_INFO, "ROUND ", ST_BAR_MUTED);
    snprintf(text, sizeof(text), "%d", game->roundNumber);
    canvasPutText(x, RT_ROW_INFO, text, ST_BAR_GOLD);

    phase = phaseName(game);
    if (phase[0] != '\0') {
        snprintf(text, sizeof(text), " %s ", phase);
        canvasPutCentered(layout->innerL, width, RT_ROW_INFO, text,
                          ST_BADGE_GOLD);
    }

    mode = game->mode == MODE_CLASSIC ? "CLASSIC" : "ROGUELIKE";
    x = layout->innerR - canvasTextWidth(mode);
    canvasPutText(x, RT_ROW_INFO, mode, ST_BAR_MUTED);
    boss = dealerIsBoss(game->run.dealerId);
    if (boss) {
        x -= 8;
        canvasPutText(x, RT_ROW_INFO, " BOSS ", ST_BADGE_RED);
    }
}

static void drawRule(const TableLayout *layout, int row) {
    canvasHLine(layout->innerL, row, layout->fw - 2, renderGlyphs()->rule,
                ST_FELT_BRASS);
}

static void formatDealerTotal(const Game *game, int hidden, char *out,
                              size_t size, int *bust) {
    int first;
    int second;

    *bust = 0;
    if (hidden || dealerHidesTotal(game->run.dealerId)) {
        snprintf(out, size, game->dealer.handCount <= 1 ? "??" : "?? / ??");
        return;
    }
    first = calculateDealerHandValue(game, &game->dealer.hands[0],
                                     game->run.dealerId);
    if (game->dealer.handCount <= 1) {
        snprintf(out, size, "%d", first);
        *bust = first > 21;
        return;
    }
    second = calculateDealerHandValue(game, &game->dealer.hands[1],
                                      game->run.dealerId);
    snprintf(out, size, "%d / %d", first, second);
    *bust = first > 21 && second > 21;
}

static void drawHeaders(const Game *game, const TableLayout *layout,
                        int hidden) {
    const Glyphs *glyphs;
    const DealerInfo *dealerInfo;
    char total[32];
    char name[96];
    int bust;
    int x;
    int rightX;
    int value;

    glyphs = renderGlyphs();
    dealerInfo = getDealerInfo(game->run.dealerId);

    drawRule(layout, RT_ROW_DEALER_RULE);
    formatDealerTotal(game, hidden, total, sizeof(total), &bust);
    rightX = layout->innerR - 8 - canvasTextWidth(total);
    canvasPutText(rightX, RT_ROW_DEALER_RULE, " TOTAL ", ST_FELT_DEALER_D);
    canvasPutText(rightX + 7, RT_ROW_DEALER_RULE, total,
                  bust ? ST_FELT_BAD : ST_FELT_DEALER);
    canvasPutText(rightX + 7 + canvasTextWidth(total), RT_ROW_DEALER_RULE,
                  " ", ST_FELT_TEXT);

    x = layout->innerL + 1;
    x += canvasPutText(x, RT_ROW_DEALER_RULE, " ", ST_FELT_TEXT);
    x += canvasPutText(x, RT_ROW_DEALER_RULE, glyphs->dealer,
                       ST_FELT_DEALER);
    x += canvasPutText(x, RT_ROW_DEALER_RULE, " DEALER", ST_FELT_DEALER);
    if (dealerInfo != NULL && dealerInfo->name != NULL) {
        snprintf(name, sizeof(name), "  %s", dealerInfo->name);
        x += canvasPutFit(x, RT_ROW_DEALER_RULE, rightX - 2 - x, name,
                          ST_FELT_DEALER_D);
    }
    canvasPutText(x, RT_ROW_DEALER_RULE, " ", ST_FELT_TEXT);

    drawRule(layout, RT_ROW_PLAYER_RULE);
    value = calculatePlayerHandValueForIndex(game, handIndexToShow(game));
    snprintf(total, sizeof(total), "%d", value);
    rightX = layout->innerR - 8 - canvasTextWidth(total);
    canvasPutText(rightX, RT_ROW_PLAYER_RULE, " TOTAL ", ST_FELT_PLAYER_D);
    canvasPutText(rightX + 7, RT_ROW_PLAYER_RULE, total,
                  value > 21 ? ST_FELT_BAD : ST_FELT_PLAYER);
    canvasPutText(rightX + 7 + canvasTextWidth(total), RT_ROW_PLAYER_RULE,
                  " ", ST_FELT_TEXT);
    x = layout->innerL + 1;
    x += canvasPutText(x, RT_ROW_PLAYER_RULE, " ", ST_FELT_TEXT);
    x += canvasPutText(x, RT_ROW_PLAYER_RULE, glyphs->player,
                       ST_FELT_PLAYER);
    x += canvasPutText(x, RT_ROW_PLAYER_RULE, " PLAYER", ST_FELT_PLAYER);
    canvasPutText(x, RT_ROW_PLAYER_RULE, " ", ST_FELT_TEXT);
}

static void composeTable(const Game *game, int hideDealerCard,
                         const TableLayout *layout, const Scene *scene) {
    int hidden;
    int deckLeft;
    int totalBet;
    int index;
    int hands;

    renderLayoutSetCurrent(layout);
    hidden = dealerHidden(game, hideDealerCard);

    canvasBlank();
    drawFelt(layout);
    drawFrame(layout);
    drawInfoBar(game, layout);
    drawHeaders(game, layout, hidden);

    if (layout->gutter > 0) {
        deckLeft = scene->deckLeft >= 0 ? scene->deckLeft
                                        : DECK_SIZE - game->deck.top;
        renderShoe(layout, deckLeft);
        totalBet = 0;
        hands = game->player.handCount;
        if (hands > RMAXH) {
            hands = RMAXH;
        }
        for (index = 0; index < hands; index++) {
            totalBet += game->player.bets[index];
        }
        renderChipPot(layout, totalBet,
                      getMinBet(game, game->run.floor, game->mode));
    }

    renderCardsDrawSide(game, SIDE_DEALER, layout, scene, hidden);
    renderCardsDrawSide(game, SIDE_PLAYER, layout, scene, 0);
    renderCardsDrawBadges(game, layout, scene);
    if (scene->fly) {
        renderCardSprite(layout, &BLANK_CARD, 1, scene->flyX, scene->flyY,
                         0, ST_CARD_EDGE);
    }

    renderStatusRow(game,
                    scene->chipsShown >= 0 ? scene->chipsShown
                                           : game->player.chips,
                    scene->chipDelta, layout->gutter == 0);
    renderTableInfoRows(game, tablePadding);
    renderLogRows(game, tablePadding, layout->termW);
}

typedef struct {
    const Game *game;
    int column;
} PresentContext;

/* Runs inside the synchronized-update bracket, after the canvas cells. */
static void presentExtras(const void *context) {
    const PresentContext *present;

    present = (const PresentContext *)context;
    renderOracleInfo(present->game, present->column);
    printf(SCR_CURSOR, ROW_PROMPT, present->column + 1);
}

static void present(const Game *game, const TableLayout *layout) {
    PresentContext context;

    context.game = game;
    context.column = layout->fl - 1;
    canvasFlushEx(presentExtras, &context);
}

/* ------------------------------------------------------------------ */
/* Animations                                                          */
/* ------------------------------------------------------------------ */

#define DEAL_FRAMES 6
#define DEAL_FRAME_MS 22
#define FLIP_FRAME_MS 26
#define CHIP_FRAMES 6
#define CHIP_HOLD 3
#define CHIP_FRAME_MS 28
#define MAX_ANIMATED_CARDS 12

typedef struct {
    int side;
    int hand;
    int index;
} NewCard;

static int easeOut(int step, int steps) {
    int inverse;

    inverse = 256 - (step * 256) / steps;
    return 256 - (inverse * inverse * inverse) / 65536;
}

static int animationsAllowed(const Game *game) {
    return game != NULL && !game->fastMode &&
           isatty(STDOUT_FILENO) && renderMotionEnabled();
}

static void takeSnapshot(FrameMemory *snapshot, const Game *game,
                         int hidden) {
    int index;

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->valid = 1;
    snapshot->round = game->roundNumber;
    snapshot->handCount[SIDE_DEALER] = game->dealer.handCount;
    snapshot->handCount[SIDE_PLAYER] = game->player.handCount;
    for (index = 0; index < RMAXH; index++) {
        if (index < game->dealer.handCount) {
            snapshot->count[SIDE_DEALER][index] =
                game->dealer.hands[index].count;
            snapshot->total[SIDE_DEALER] +=
                game->dealer.hands[index].count;
        }
        if (index < game->player.handCount) {
            snapshot->count[SIDE_PLAYER][index] =
                game->player.hands[index].count;
            snapshot->total[SIDE_PLAYER] +=
                game->player.hands[index].count;
        }
    }
    snapshot->dealerHidden = hidden;
    if (game->dealer.handCount > 0 && game->dealer.hands[0].count > 0) {
        snapshot->hole = game->dealer.hands[0].cards[0];
    }
    snapshot->chips = game->player.chips;
}

static void applyCounts(Game *work, int (*counts)[RMAXH]) {
    int index;

    for (index = 0; index < work->dealer.handCount && index < RMAXH;
         index++) {
        work->dealer.hands[index].count = counts[SIDE_DEALER][index];
    }
    for (index = 0; index < work->player.handCount && index < RMAXH;
         index++) {
        work->player.hands[index].count = counts[SIDE_PLAYER][index];
    }
}

static int lerpInt(int from, int to, int t256) {
    return from + ((to - from) * t256) / 256;
}

static void animateDeals(const Game *game, Game *work, int hide,
                         const TableLayout *layout,
                         const FrameMemory *current, int sameRound) {
    NewCard cards[MAX_ANIMATED_CARDS];
    int base[2][RMAXH];
    int running[2][RMAXH];
    int total;
    int side;
    int hand;
    int index;
    int step;
    int frame;
    int turn;
    int t;
    int originX;
    int originY;
    int targetX;
    int targetY;
    int xTo[RMAXH][RMAXC];
    int playerIdx[MAX_ANIMATED_CARDS];
    int dealerIdx[MAX_ANIMATED_CARDS];
    int playerCount;
    int dealerCount;
    int playerPos;
    int dealerPos;
    int finalLeft;
    Scene scene;
    NewCard picked[MAX_ANIMATED_CARDS];

    memset(base, 0, sizeof(base));
    if (sameRound &&
        memory.handCount[SIDE_DEALER] == current->handCount[SIDE_DEALER] &&
        memory.handCount[SIDE_PLAYER] == current->handCount[SIDE_PLAYER]) {
        memcpy(base, memory.count, sizeof(base));
    } else if (sameRound) {
        return; /* a split reshuffled the hands: nothing was dealt */
    }

    total = 0;
    playerCount = 0;
    dealerCount = 0;
    for (side = 0; side < 2; side++) {
        for (hand = 0; hand < current->handCount[side] && hand < RMAXH;
             hand++) {
            for (index = base[side][hand];
                 index < current->count[side][hand]; index++) {
                if (total >= MAX_ANIMATED_CARDS) {
                    return;
                }
                cards[total].side = side;
                cards[total].hand = hand;
                cards[total].index = index;
                if (side == SIDE_PLAYER) {
                    playerIdx[playerCount++] = total;
                } else {
                    dealerIdx[dealerCount++] = total;
                }
                total++;
            }
        }
    }
    if (total == 0) {
        return;
    }

    /* Dealing order: player, dealer, player, dealer, ... */
    step = 0;
    playerPos = 0;
    dealerPos = 0;
    while (playerPos < playerCount || dealerPos < dealerCount) {
        if (playerPos < playerCount) {
            picked[step++] = cards[playerIdx[playerPos++]];
        }
        if (dealerPos < dealerCount) {
            picked[step++] = cards[dealerIdx[dealerPos++]];
        }
    }

    memcpy(running, base, sizeof(running));
    finalLeft = DECK_SIZE - game->deck.top;
    if (finalLeft < 0) {
        finalLeft = 0;
    }
    originX = layout->gutter > 0 ? layout->shoeX
                                 : layout->innerR - layout->cardW + 1;
    originY = layout->gutter > 0 ? layout->shoeY : RT_ROW_INFO;

    for (turn = 0; turn < total; turn++) {
        renderSceneFromGame(game, &scene);
        memcpy(scene.from, running, sizeof(scene.from));
        memcpy(scene.to, running, sizeof(scene.to));
        scene.to[picked[turn].side][picked[turn].hand]++;
        scene.deckLeft = finalLeft + (total - 1 - turn);

        renderLayoutHandsX(layout, scene.to[picked[turn].side],
                           current->handCount[picked[turn].side], xTo);
        targetX = xTo[picked[turn].hand][picked[turn].index];
        targetY = (picked[turn].side == SIDE_DEALER ? RT_ROW_DEALER_CARDS
                                                    : RT_ROW_PLAYER_CARDS) +
                  layout->cardYOff;

        applyCounts(work, scene.from);
        for (frame = 1; frame <= DEAL_FRAMES; frame++) {
            t = easeOut(frame, DEAL_FRAMES);
            scene.t256 = t;
            scene.fly = 1;
            scene.flyX = lerpInt(originX, targetX, t);
            scene.flyY = lerpInt(originY, targetY, t);
            composeTable(work, hide, layout, &scene);
            present(game, layout);
            renderPause(game, DEAL_FRAME_MS);
        }
        memcpy(running, scene.to, sizeof(running));
    }
}

static void animateFlip(const Game *game, int hide,
                        const TableLayout *layout) {
    static const int WIDTHS_FULL[] = { 9, 7, 5, 3, 1, 3, 5, 7, 9 };
    static const int WIDTHS_COMPACT[] = { 5, 3, 1, 3, 5 };
    const int *widths;
    int count;
    int index;
    int half;
    Scene scene;

    widths = layout->compact ? WIDTHS_COMPACT : WIDTHS_FULL;
    count = layout->compact ? 5 : 9;
    half = count / 2;
    for (index = 0; index < count; index++) {
        renderSceneFromGame(game, &scene);
        scene.flip = 1;
        scene.flipW = widths[index];
        scene.flipFaceUp = index > half;
        /* totals stay hidden until the card is face up again */
        composeTable(game, index > half ? hide : 1, layout, &scene);
        present(game, layout);
        renderPause(game, FLIP_FRAME_MS);
    }
}

static void animateChips(const Game *game, int hide, int fromChips,
                         const TableLayout *layout) {
    int delta;
    int frame;
    Scene scene;

    delta = game->player.chips - fromChips;
    if (delta == 0) {
        return;
    }
    for (frame = 1; frame <= CHIP_FRAMES + CHIP_HOLD; frame++) {
        renderSceneFromGame(game, &scene);
        scene.chipsShown = frame >= CHIP_FRAMES
                               ? game->player.chips
                               : fromChips +
                                     (delta * easeOut(frame, CHIP_FRAMES)) /
                                         256;
        scene.chipDelta = delta;
        composeTable(game, hide, layout, &scene);
        present(game, layout);
        renderPause(game, CHIP_FRAME_MS);
    }
}

/* Plays whatever changed since the last frame. */
static void animateChanges(const Game *game, int hide, int hidden,
                           const TableLayout *layout) {
    FrameMemory current;
    Game *work;
    int sameRound;
    int flip;

    if (!memory.valid || !animationsAllowed(game)) {
        return;
    }
    takeSnapshot(&current, game, hidden);
    sameRound = memory.round == current.round &&
                memory.total[SIDE_DEALER] <= current.total[SIDE_DEALER] &&
                memory.total[SIDE_PLAYER] <= current.total[SIDE_PLAYER];

    flip = sameRound && memory.dealerHidden && !hidden &&
           current.handCount[SIDE_DEALER] > 0 &&
           memory.count[SIDE_DEALER][0] > 0 &&
           current.count[SIDE_DEALER][0] > 0 &&
           memory.hole.rank == current.hole.rank &&
           memory.hole.suit == current.hole.suit;

    work = (Game *)malloc(sizeof(*work));
    if (work != NULL) {
        memcpy(work, game, sizeof(*work));
        animateDeals(game, work, hide, layout, &current, sameRound);
        free(work);
    }
    if (flip) {
        animateFlip(game, hide, layout);
    }
    if (current.chips != memory.chips) {
        animateChips(game, hide, memory.chips, layout);
    }
}

/* ------------------------------------------------------------------ */
/* Public entry points                                                 */
/* ------------------------------------------------------------------ */

void renderTable(const Game *game, int hideDealerCard) {
    TableLayout layout;
    Scene scene;
    int terminalWidth;
    int terminalHeight;
    int hidden;

    renderInitColors();
    hidden = dealerHidden(game, hideDealerCard);

    terminalGetSize(&terminalWidth, &terminalHeight);

    if (isatty(STDOUT_FILENO) &&
        !renderTerminalSizeOk(&terminalWidth,
                              &terminalHeight)) {
        renderTooSmallScreen(terminalWidth, terminalHeight);
        canvasInvalidate();
        return;
    }

    if (isatty(STDOUT_FILENO) && canvasBegin() &&
        renderLayoutCompute(game, terminalWidth, terminalHeight,
                            &layout)) {
        renderHideCursor();
        tablePadding = layout.fl - 1;
        renderLayoutSetCurrent(&layout);

        animateChanges(game, hideDealerCard, hidden, &layout);

        renderSceneFromGame(game, &scene);
        composeTable(game, hideDealerCard, &layout, &scene);
        canvasInvalidate();
        present(game, &layout);
        takeSnapshot(&memory, game, hidden);
    } else if (isatty(STDOUT_FILENO)) {
        if (renderCursorAddressable()) {
            printf("%s%s%s", scrReset(), SCR_CLEAR, SCR_HOME);
        }
        renderGameStateFallback(game, hideDealerCard);
        fflush(stdout);
        canvasInvalidate();
    } else {
        renderGameStateFallback(game, hideDealerCard);
    }
}

void renderMenuLogRefresh(const Game *game) {
    int terminalWidth;
    int terminalHeight;

    if (!isatty(STDOUT_FILENO)) {
        return;
    }

    if (!renderTerminalSizeOk(&terminalWidth,
                              &terminalHeight)) {
        return;
    }

    if (renderLayoutCurrent() == NULL || !canvasBegin()) {
        return;
    }
    renderStatusRow(game, game->player.chips, 0,
                    renderLayoutCurrent()->gutter == 0);
    renderLogRows(game, tablePadding, terminalWidth);
    canvasFlush();
}
