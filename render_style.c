#define _DEFAULT_SOURCE
#include "render_style.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int colorEnabled = 1;
static int colorDepth;
static int utf8Enabled = 1;
static int addressable;
static int motionEnabled = 1;
static int cursorHidden;

int renderTerminalSupportsAnsi(int stdoutIsTerminal,
                               const char *term,
                               const char *noColor) {
    return stdoutIsTerminal &&
           noColor == NULL &&
           term != NULL &&
           term[0] != '\0' &&
           strcmp(term, "dumb") != 0;
}

int renderCursorAddressableFor(int stdoutIsTerminal, const char *term) {
    return stdoutIsTerminal &&
           term != NULL &&
           term[0] != '\0' &&
           strcmp(term, "dumb") != 0;
}

static int containsNoCase(const char *text, const char *needle) {
    size_t needleLength;
    size_t index;

    needleLength = strlen(needle);
    for (; *text != '\0'; text++) {
        for (index = 0; index < needleLength; index++) {
            if (text[index] == '\0' ||
                tolower((unsigned char)text[index]) !=
                    tolower((unsigned char)needle[index])) {
                break;
            }
        }
        if (index == needleLength) {
            return 1;
        }
    }
    return 0;
}

static int detectUtf8(void) {
    const char *forced;
    const char *locale;

    forced = getenv("TTY21_ASCII");
    if (forced != NULL && forced[0] != '\0' && strcmp(forced, "0") != 0) {
        return 0;
    }
    locale = getenv("LC_ALL");
    if (locale == NULL || locale[0] == '\0') {
        locale = getenv("LC_CTYPE");
    }
    if (locale == NULL || locale[0] == '\0') {
        locale = getenv("LANG");
    }
    if (locale == NULL || locale[0] == '\0') {
        return 1;
    }
    return containsNoCase(locale, "utf-8") || containsNoCase(locale, "utf8");
}

static int detectDepth(const char *term) {
    const char *forced;
    const char *colorterm;

    forced = getenv("TTY21_COLORS");
    if (forced != NULL && forced[0] != '\0') {
        if (strcmp(forced, "256") == 0) return 256;
        if (strcmp(forced, "16") == 0) return 16;
        if (strcmp(forced, "0") == 0) return 0;
    }
    colorterm = getenv("COLORTERM");
    if (colorterm != NULL && colorterm[0] != '\0') {
        return 256;
    }
    if (term != NULL && strstr(term, "256color") != NULL) {
        return 256;
    }
    return 16;
}

void renderInitColors(void) {
    static int initialized;
    const char *term;
    const char *noAnim;
    int isTerminal;

    if (initialized) {
        return;
    }
    term = getenv("TERM");
    isTerminal = isatty(STDOUT_FILENO);
    colorEnabled = renderTerminalSupportsAnsi(isTerminal, term,
                                              getenv("NO_COLOR"));
    addressable = renderCursorAddressableFor(isTerminal, term);
    colorDepth = colorEnabled ? detectDepth(term) : 0;
    if (colorDepth == 0) {
        colorEnabled = 0;
    }
    utf8Enabled = detectUtf8();
    noAnim = getenv("TTY21_NO_ANIM");
    motionEnabled = !(noAnim != NULL && noAnim[0] != '\0' &&
                      strcmp(noAnim, "0") != 0);
    initialized = 1;
}

int renderAnsiEnabled(void) {
    renderInitColors();
    return colorEnabled;
}

int renderCursorAddressable(void) {
    renderInitColors();
    return addressable;
}

int renderColorDepth(void) {
    renderInitColors();
    return colorDepth;
}

int renderUtf8Enabled(void) {
    renderInitColors();
    return utf8Enabled;
}

int renderMotionEnabled(void) {
    renderInitColors();
    return motionEnabled;
}

const char *scrReset(void) {
    return renderAnsiEnabled() ? "\033[0m" : "";
}

void renderHideCursor(void) {
    if (renderCursorAddressable() && !cursorHidden) {
        printf("\033[?25l");
        fflush(stdout);
        cursorHidden = 1;
    }
}

void renderShowCursor(void) {
    if (renderCursorAddressable() && cursorHidden) {
        printf("\033[?25h");
        fflush(stdout);
        cursorHidden = 0;
    }
}

const char *scrRed(void) {
    return renderAnsiEnabled() ? "\033[31m" : "";
}

const char *scrGreen(void) {
    return renderAnsiEnabled() ? "\033[32m" : "";
}

const char *scrYellow(void) {
    return renderAnsiEnabled() ? "\033[33m" : "";
}

const char *scrCyan(void) {
    return renderAnsiEnabled() ? "\033[36m" : "";
}

const char *scrMagenta(void) {
    return renderAnsiEnabled() ? "\033[35m" : "";
}

const char *scrBold(void) {
    return renderAnsiEnabled() ? "\033[1m" : "";
}

const char *scrDim(void) {
    return renderAnsiEnabled() ? "\033[2m" : "";
}

const char *scrAccent(void) { return scrCyan(); }
const char *scrSuccess(void) { return scrGreen(); }
const char *scrWarning(void) { return scrYellow(); }
const char *scrDanger(void) { return scrRed(); }
const char *scrMuted(void) { return scrDim(); }
const char *scrEmphasis(void) { return scrBold(); }

/* ------------------------------------------------------------------ */
/* Palette                                                             */
/* ------------------------------------------------------------------ */

#define AT_BOLD 1
#define AT_DIM 2

typedef struct {
    short fg;      /* xterm-256 index or -1 */
    short bg;
    short fg16;    /* 0-15 or -1 */
    short bg16;
    unsigned char attr;
} StyleDef;

/* Felt green is 22, brass 136/178, ivory 230, navy card back 17. */
static const StyleDef STYLES[ST_COUNT] = {
    [ST_PLAIN]           = { -1, -1, -1, -1, 0 },
    [ST_MUTED]           = { 245, -1, -1, -1, AT_DIM },
    [ST_STRONG]          = { 255, -1, -1, -1, AT_BOLD },
    [ST_BRASS]           = { 136, -1, 3, -1, 0 },
    [ST_GOLD]            = { 178, -1, 11, -1, 0 },
    [ST_GOLD_B]          = { 221, -1, 11, -1, AT_BOLD },
    [ST_OK]              = { 78, -1, 10, -1, 0 },
    [ST_BAD]             = { 203, -1, 9, -1, 0 },
    [ST_WARN]            = { 221, -1, 11, -1, 0 },
    [ST_ACCENT]          = { 80, -1, 14, -1, 0 },
    [ST_ROSE]            = { 175, -1, 13, -1, 0 },
    [ST_FELT]            = { 28, 22, 2, 2, 0 },
    [ST_FELT_TEXT]       = { 230, 22, 15, 2, 0 },
    [ST_FELT_MUTED]      = { 108, 22, 7, 2, 0 },
    [ST_FELT_BRASS]      = { 178, 22, 11, 2, 0 },
    [ST_FELT_GOLD]       = { 221, 22, 11, 2, AT_BOLD },
    [ST_FELT_DEALER]     = { 217, 22, 13, 2, AT_BOLD },
    [ST_FELT_DEALER_D]   = { 175, 22, 5, 2, 0 },
    [ST_FELT_PLAYER]     = { 123, 22, 14, 2, AT_BOLD },
    [ST_FELT_PLAYER_D]   = { 79, 22, 6, 2, 0 },
    [ST_FELT_OK]         = { 120, 22, 10, 2, AT_BOLD },
    [ST_FELT_BAD]        = { 210, 22, 9, 2, AT_BOLD },
    [ST_BAR]             = { 250, 235, 7, 0, 0 },
    [ST_BAR_GOLD]        = { 221, 235, 11, 0, AT_BOLD },
    [ST_BAR_MUTED]       = { 244, 235, 8, 0, 0 },
    [ST_BADGE_GOLD]      = { 16, 178, 0, 11, AT_BOLD },
    [ST_BADGE_RED]       = { 231, 124, 15, 1, AT_BOLD },
    [ST_BADGE_OK]        = { 16, 78, 0, 10, AT_BOLD },
    [ST_BADGE_WARN]      = { 16, 172, 0, 3, AT_BOLD },
    [ST_BADGE_DEALER]    = { 16, 217, 0, 13, AT_BOLD },
    [ST_BADGE_PLAYER]    = { 16, 123, 0, 14, AT_BOLD },
    [ST_CARD_RED]        = { 160, 230, 1, 15, 0 },
    [ST_CARD_BLACK]      = { 234, 230, 0, 15, 0 },
    [ST_CARD_EDGE]       = { 246, 230, 8, 15, 0 },
    [ST_CARD_EDGE_ACTIVE]= { 172, 230, 3, 15, AT_BOLD },
    [ST_CARD_BLEED]      = { 124, 230, 1, 15, AT_BOLD },
    [ST_CARD_FROST]      = { 25, 230, 4, 15, AT_BOLD },
    [ST_CARD_ECHO]       = { 130, 230, 3, 15, AT_BOLD },
    [ST_CARD_BACK]       = { 68, 17, 12, 4, 0 },
    [ST_CARD_BACK_EDGE]  = { 136, 17, 11, 4, 0 },
    [ST_CARD_BACK_BRAND] = { 178, 17, 11, 4, AT_BOLD },
    [ST_SLOT]            = { 29, 22, 2, 2, 0 },
    [ST_CHIP_RED]        = { 231, 124, 15, 1, 0 },
    [ST_CHIP_BLUE]       = { 231, 25, 15, 4, 0 },
    [ST_CHIP_GREEN]      = { 231, 29, 15, 6, 0 },
    [ST_CHIP_GOLD]       = { 16, 178, 0, 11, 0 },
    [ST_CHIP_BLACK]      = { 252, 237, 15, 0, 0 },
    [ST_SEL]             = { 16, 178, 0, 11, AT_BOLD },
    [ST_SEL_KEY]         = { 52, 178, 1, 11, AT_BOLD },
    [ST_KEYCAP]          = { 221, 237, 11, 0, AT_BOLD },
};

int renderStyleSgr(RenderStyle style, char *buf, size_t size) {
    const StyleDef *def;
    unsigned int id;
    int length;
    int depth;
    int attrDim;

    if (buf == NULL || size == 0) {
        return 0;
    }
    buf[0] = '\0';
    if (!renderAnsiEnabled() || size < 48) {
        return 0;
    }

    id = (unsigned int)(style & 0xFFu);
    if (id >= (unsigned int)ST_COUNT) {
        id = 0;
    }
    def = &STYLES[id];
    depth = renderColorDepth();
    attrDim = (def->attr & AT_DIM) != 0 || (style & RSTYLE_DIM) != 0;

    length = snprintf(buf, size, "\033[0");
    if ((def->attr & AT_BOLD) != 0) {
        length += snprintf(buf + length, size - (size_t)length, ";1");
    }
    if (attrDim) {
        length += snprintf(buf + length, size - (size_t)length, ";2");
    }
    if (depth >= 256) {
        if (def->fg >= 0) {
            length += snprintf(buf + length, size - (size_t)length,
                               ";38;5;%d", def->fg);
        }
        if (def->bg >= 0) {
            length += snprintf(buf + length, size - (size_t)length,
                               ";48;5;%d", def->bg);
        }
    } else {
        if (def->fg16 >= 0) {
            length += snprintf(buf + length, size - (size_t)length, ";%d",
                               def->fg16 < 8 ? 30 + def->fg16
                                             : 90 + (def->fg16 - 8));
        }
        if (def->bg16 >= 0) {
            length += snprintf(buf + length, size - (size_t)length, ";%d",
                               def->bg16 < 8 ? 40 + def->bg16
                                             : 100 + (def->bg16 - 8));
        }
    }
    snprintf(buf + length, size - (size_t)length, "m");
    return 1;
}
