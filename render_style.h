#ifndef RENDER_STYLE_H
#define RENDER_STYLE_H

#include <stddef.h>

/* ---- Original colour helpers (unchanged API) ---- */
const char *scrReset(void);
const char *scrGreen(void);
const char *scrYellow(void);
const char *scrMagenta(void);
const char *scrBold(void);
const char *scrDim(void);
const char *scrCyan(void);
const char *scrRed(void);
void renderInitColors(void);
void renderHideCursor(void);
void renderShowCursor(void);
int renderAnsiEnabled(void);
int renderTerminalSupportsAnsi(int stdoutIsTerminal,
                               const char *term,
                               const char *noColor);
const char *scrAccent(void);
const char *scrSuccess(void);
const char *scrWarning(void);
const char *scrDanger(void);
const char *scrMuted(void);
const char *scrEmphasis(void);

/* ---- Terminal capabilities ---- */
/* 1 when stdout is a terminal that understands cursor addressing. */
int renderCursorAddressable(void);
int renderCursorAddressableFor(int stdoutIsTerminal, const char *term);
/* 0 = no colour, 16 = basic ANSI, 256 = xterm palette. */
int renderColorDepth(void);
/* 1 when box drawing / suit glyphs can be used. */
int renderUtf8Enabled(void);
/* 0 when TTY21_NO_ANIM is set. */
int renderMotionEnabled(void);

/* ---- Palette ("velvet and brass") ----
 * Every on-screen element uses one of these ids. A style is an id
 * optionally OR-ed with RSTYLE_DIM. */
typedef enum RenderStyleId {
    ST_PLAIN = 0,
    ST_MUTED,
    ST_STRONG,
    ST_BRASS,
    ST_GOLD,
    ST_GOLD_B,
    ST_OK,
    ST_BAD,
    ST_WARN,
    ST_ACCENT,
    ST_ROSE,
    ST_FELT,
    ST_FELT_TEXT,
    ST_FELT_MUTED,
    ST_FELT_BRASS,
    ST_FELT_GOLD,
    ST_FELT_DEALER,
    ST_FELT_DEALER_D,
    ST_FELT_PLAYER,
    ST_FELT_PLAYER_D,
    ST_FELT_OK,
    ST_FELT_BAD,
    ST_BAR,
    ST_BAR_GOLD,
    ST_BAR_MUTED,
    ST_BADGE_GOLD,
    ST_BADGE_RED,
    ST_BADGE_OK,
    ST_BADGE_WARN,
    ST_BADGE_DEALER,
    ST_BADGE_PLAYER,
    ST_CARD_RED,
    ST_CARD_BLACK,
    ST_CARD_EDGE,
    ST_CARD_EDGE_ACTIVE,
    ST_CARD_BLEED,
    ST_CARD_FROST,
    ST_CARD_ECHO,
    ST_CARD_BACK,
    ST_CARD_BACK_EDGE,
    ST_CARD_BACK_BRAND,
    ST_SLOT,
    ST_CHIP_RED,
    ST_CHIP_BLUE,
    ST_CHIP_GREEN,
    ST_CHIP_GOLD,
    ST_CHIP_BLACK,
    ST_SEL,
    ST_SEL_KEY,
    ST_KEYCAP,
    ST_COUNT
} RenderStyleId;

#define RSTYLE_DIM 0x100
typedef unsigned short RenderStyle;

/* Writes the SGR sequence selecting `style` into buf. Writes an empty
 * string (and returns 0) when colour output is disabled. */
int renderStyleSgr(RenderStyle style, char *buf, size_t size);

#endif
