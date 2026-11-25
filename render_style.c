#define _DEFAULT_SOURCE
#include "render_style.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int colorEnabled = 1;
static int cursorHidden;

// Keep ANSI capability decisions consistent and independently testable.
int renderTerminalSupportsAnsi(int stdoutIsTerminal,
                               const char *term,
                               const char *noColor) {
    return stdoutIsTerminal &&
           noColor == NULL &&
           term != NULL &&
           term[0] != '\0' &&
           strcmp(term, "dumb") != 0;
}

// Reads NO_COLOR and terminal capability once before the first visible render.
void renderInitColors(void) {
    static int initialized;

    if (!initialized) {
        colorEnabled = renderTerminalSupportsAnsi(
            isatty(STDOUT_FILENO), getenv("TERM"), getenv("NO_COLOR"));
        initialized = 1;
    }
}

// Reports whether ANSI styling and cursor control are safe for stdout.
int renderAnsiEnabled(void) {
    renderInitColors();
    return colorEnabled;
}

const char *scrReset(void) {
    return renderAnsiEnabled() ? "\033[0m" : "";
}

void renderHideCursor(void) {
    if (renderAnsiEnabled() && !cursorHidden) {
        printf("\033[?25l");
        fflush(stdout);
        cursorHidden = 1;
    }
}

void renderShowCursor(void) {
    if (renderAnsiEnabled() && cursorHidden) {
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

// Semantic palette: renderers describe meaning, not ANSI color numbers.
const char *scrAccent(void) { return scrCyan(); }
const char *scrSuccess(void) { return scrGreen(); }
const char *scrWarning(void) { return scrYellow(); }
const char *scrDanger(void) { return scrRed(); }
const char *scrMuted(void) { return scrDim(); }
const char *scrEmphasis(void) { return scrBold(); }
