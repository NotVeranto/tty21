#define _DEFAULT_SOURCE
#include "render_terminal.h"
#include "render_control.h"
#include "render_style.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

int renderTerminalDimensionsOk(int width, int height) {
    return terminalDimensionsOk(width, height);
}

int renderTerminalSizeOk(int *width, int *height) {
    terminalGetSize(width, height);
    return renderTerminalDimensionsOk(*width, *height);
}

void renderTooSmallScreen(int currentWidth,
                          int currentHeight) {
    const char *msg;
    char sized[128];
    char current[32];
    int lines;
    int row;
    int useAnsi;

    useAnsi = renderAnsiEnabled();

    if (currentWidth <= 0 || currentHeight <= 0) {
        fprintf(stderr,
                "error: terminal too small.\n"
                "tty21 needs at least %dx%d.\n",
                MIN_TERMINAL_WIDTH, MIN_TERMINAL_HEIGHT);
        return;
    }

    if (currentWidth >= 40 && currentHeight >= 6) {
        msg =
            "Terminal too small.\n"
            "\n"
            "Current: %dx%d\n"
            "Minimum: %dx%d\n"
            "\n"
            "Resize the window to continue.";
        snprintf(sized, sizeof(sized), msg,
                 currentWidth, currentHeight,
                 MIN_TERMINAL_WIDTH, MIN_TERMINAL_HEIGHT);
        lines = 6;
    } else if (currentWidth >= 24 && currentHeight >= 3) {
        snprintf(sized, sizeof(sized),
                 "Need %dx%d (got %dx%d)",
                 MIN_TERMINAL_WIDTH, MIN_TERMINAL_HEIGHT,
                 currentWidth, currentHeight);
        lines = 1;
    } else {
        snprintf(current, sizeof(current),
                 "%dx%d", currentWidth, currentHeight);
        msg = current;
        snprintf(sized, sizeof(sized), "%s", msg);
        lines = 1;
    }

    if (useAnsi) {
        printf("%s%s", SCR_CLEAR, SCR_HOME);
    }

    row = (currentHeight - lines) / 2;
    if (row < 1) {
        row = 1;
    }

    {
        // Split sized on '\n' and center each line.
        char *cursor;
        char *start;
        int lineNo;

        cursor = sized;
        lineNo = 0;

        while (*cursor != '\0' &&
               lineNo < lines) {
            char *nl;
            int len;
            int pad;

            start = cursor;
            nl = strchr(cursor, '\n');

            if (nl != NULL) {
                *nl = '\0';
            }

            len = (int)strlen(start);

            if (len > currentWidth) {
                start[currentWidth] = '\0';
                len = currentWidth;
            }

            pad = (currentWidth - len) / 2;
            if (pad < 0) {
                pad = 0;
            }

            if (useAnsi) {
                printf(SCR_CURSOR, row + lineNo, pad + 1);
            } else {
                int k;
                for (k = 0; k < pad; k++) {
                    putchar(' ');
                }
            }

            printf("%s", start);

            if (!useAnsi) {
                putchar('\n');
            }

            lineNo++;

            if (nl == NULL) {
                break;
            }

            cursor = nl + 1;
        }
    }

    fflush(stdout);
}
