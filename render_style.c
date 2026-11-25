#define _DEFAULT_SOURCE
#include "render_style.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int colorEnabled = 1;
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

void renderInitColors(void) {
    static int initialized;

    if (!initialized) {
        colorEnabled = renderTerminalSupportsAnsi(
            isatty(STDOUT_FILENO), getenv("TERM"), getenv("NO_COLOR"));
        initialized = 1;
    }
}

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

const char *scrAccent(void) { return scrCyan(); }
const char *scrSuccess(void) { return scrGreen(); }
const char *scrWarning(void) { return scrYellow(); }
const char *scrDanger(void) { return scrRed(); }
const char *scrMuted(void) { return scrDim(); }
const char *scrEmphasis(void) { return scrBold(); }

const char *scrCardBorder(void) {
    return renderAnsiEnabled() ? "\033[38;5;250m" : "";
}

const char *scrCardFace(void) {
    return renderAnsiEnabled() ? "\033[48;5;255m" : "";
}

const char *scrCardRedInk(void) {
    return renderAnsiEnabled() ? "\033[38;5;160m" : "";
}

const char *scrCardBlackInk(void) {
    return renderAnsiEnabled() ? "\033[38;5;235m" : "";
}

const char *scrCardBack(void) {
    return renderAnsiEnabled() ? "\033[48;5;24m" : "";
}

const char *scrCardBackInk(void) {
    return renderAnsiEnabled() ? "\033[38;5;117m" : "";
}
