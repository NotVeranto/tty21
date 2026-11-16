#define _DEFAULT_SOURCE
#include "terminal.h"

#include <sys/ioctl.h>
#include <unistd.h>

// Reads terminal dimensions and falls back to a standard terminal size.
void terminalGetSize(int *width, int *height) {
    struct winsize windowSize;

    *width = 80;
    *height = 24;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &windowSize) == 0) {
        if (windowSize.ws_col > 0) {
            *width = windowSize.ws_col;
        }

        if (windowSize.ws_row > 0) {
            *height = windowSize.ws_row;
        }
    }
}

// Checks whether the terminal can display the full-screen game layout.
int terminalDimensionsOk(int width, int height) {
    return width >= MIN_TERMINAL_WIDTH &&
           height >= MIN_TERMINAL_HEIGHT;
}
