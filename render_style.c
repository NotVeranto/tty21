#define _DEFAULT_SOURCE
#include "render_style.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int colorEnabled = 1;
static int cursorHidden;

// Reads NO_COLOR once before the first visible render.
void renderInitColors(void) {
    static int initialized;

    if (!initialized) {
        colorEnabled = getenv("NO_COLOR") == NULL &&
                       isatty(STDOUT_FILENO);
        initialized = 1;
    }
}

// Returns the ANSI reset sequence when colors are enabled.
const char *scrReset(void) {
    return colorEnabled ? "\033[0m" : "";
}

// Hides the hardware cursor while full-screen rendering is active.
void renderHideCursor(void) {
    renderInitColors();

    if (colorEnabled && !cursorHidden) {
        printf("\033[?25l");
        fflush(stdout);
        cursorHidden = 1;
    }
}

// Restores the hardware cursor after full-screen rendering.
void renderShowCursor(void) {
    renderInitColors();

    if (colorEnabled && cursorHidden) {
        printf("\033[?25h");
        fflush(stdout);
        cursorHidden = 0;
    }
}

// Returns the ANSI red sequence when colors are enabled.
const char *scrRed(void) {
    return colorEnabled ? "\033[31m" : "";
}

// Returns the ANSI green sequence when colors are enabled.
const char *scrGreen(void) {
    return colorEnabled ? "\033[32m" : "";
}

// Returns the ANSI yellow sequence when colors are enabled.
const char *scrYellow(void) {
    return colorEnabled ? "\033[33m" : "";
}

// Returns the ANSI cyan sequence when colors are enabled.
const char *scrCyan(void) {
    return colorEnabled ? "\033[36m" : "";
}

// Returns the ANSI magenta sequence when colors are enabled.
const char *scrMagenta(void) {
    return colorEnabled ? "\033[35m" : "";
}

// Returns the ANSI bold sequence when colors are enabled.
const char *scrBold(void) {
    return colorEnabled ? "\033[1m" : "";
}

// Returns the ANSI dim sequence when colors are enabled.
const char *scrDim(void) {
    return colorEnabled ? "\033[2m" : "";
}
