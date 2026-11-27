#ifndef RENDER_SCENE_H
#define RENDER_SCENE_H

/*
 * Shared geometry and animation state of the table screen.
 * Internal to the render_*.c files.
 */

#include "game.h"
#include "render_canvas.h"

#define RMAXH 8     /* hands drawn per side */
#define RMAXC 16    /* cards drawn per hand */

#define SIDE_DEALER 0
#define SIDE_PLAYER 1

/* The table is a fixed grid of rows. Rows >= 21 belong to the HUD and
 * keep the numbers from render_rows.h. */
#define RT_ROW_TOP 1
#define RT_ROW_INFO 2
#define RT_ROW_DEALER_RULE 3
#define RT_ROW_DEALER_CARDS 4
#define RT_ROW_PLAYER_RULE 11
#define RT_ROW_PLAYER_CARDS 12
#define RT_ROW_BADGES 19
#define RT_ROW_BOTTOM 20
#define RT_SLOT_ROWS 7

#define RT_GUTTER 13

typedef struct {
    int termW;
    int termH;
    int compact;     /* 0: 11x7 cards, 1: 7x5 cards */
    int cardW;
    int cardH;
    int gap;
    int handGap;
    int cardYOff;    /* vertical offset of a card inside its 7-row slot */
    int fl;          /* frame left column */
    int fw;          /* frame width, borders included */
    int innerL;      /* first / last column inside the frame */
    int innerR;
    int gutter;      /* 0 or RT_GUTTER */
    int cx;          /* card area: first column and width */
    int cwid;
    int shoeX;
    int shoeY;
    int potX;
    int potY;
} TableLayout;

typedef struct {
    int from[2][RMAXH];   /* cards already on the table */
    int to[2][RMAXH];     /* cards once the current step has landed */
    int t256;             /* slide progress from -> to (0..256) */
    int fly;              /* card in flight */
    int flySide;
    int flyHand;
    int flyIdx;
    int flyX;
    int flyY;
    int flip;             /* dealer hole card turning over */
    int flipW;
    int flipFaceUp;
    int chipsShown;       /* animated chip counter, -1: game value */
    int chipDelta;        /* floating +/- next to the counter */
    int deckLeft;         /* shoe size override, -1: game value */
} Scene;

/* Layout. Returns 0 when not even the compact cards fit. */
int renderLayoutCompute(const Game *game, int termW, int termH,
                        TableLayout *out);
const TableLayout *renderLayoutCurrent(void);
void renderLayoutSetCurrent(const TableLayout *layout);
void renderLayoutForget(void);
/* Column and width the HUD rows below the frame align to. */
void renderHudRegion(int *x, int *width);
void renderLayoutHandsX(const TableLayout *layout, const int *counts,
                        int handCount, int x[RMAXH][RMAXC]);

void renderSceneFromGame(const Game *game, Scene *scene);

/* Cards and table furniture. */
void renderCardSprite(const TableLayout *layout, const Card *card,
                      int faceDown, int x, int y, int scaleW,
                      RenderStyle edge);
void renderCardsDrawSide(const Game *game, int side,
                         const TableLayout *layout, const Scene *scene,
                         int hideFirst);
void renderCardsDrawBadges(const Game *game, const TableLayout *layout,
                           const Scene *scene);
void renderShoe(const TableLayout *layout, int cardsLeft);
void renderChipPot(const TableLayout *layout, int bet, int minBet);

/* Title-screen helpers (render_menu.c). */
void renderMenuForgetSplash(void);
void renderBackdrop(int x, int y, int w, int h);

/* HUD rows (drawn into the canvas, no flush). */
void renderStatusRow(const Game *game, int displayedChips, int chipDelta,
                     int showDeck);
void renderTableInfoRows(const Game *game, int padding);
void renderLogRows(const Game *game, int padding, int terminalWidth);

#endif
