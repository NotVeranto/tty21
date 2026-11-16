#define _DEFAULT_SOURCE
#include "render_messages.h"
#include "render.h"
#include "render_style.h"
#include "render_layout.h"
#include "terminal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int messagePadding;

void renderMessagesSetPadding(int padding) {
    messagePadding = padding < 0 ? 0 : padding;
}

void renderResultFlash(const Game *game,
                       const char *message,
                       MessageStyle style) {
    const char *color;
    int terminalWidth;
    int terminalHeight;
    int column;

    if (game == NULL || message == NULL || game->fastMode ||
        !isatty(STDOUT_FILENO)) {
        return;
    }

    renderInitColors();
    if (!renderTerminalSizeOk(&terminalWidth, &terminalHeight)) {
        return;
    }

    if (style == MSG_WIN) {
        color = scrGreen();
    } else if (style == MSG_LOSS) {
        color = scrRed();
    } else if (style == MSG_PUSH) {
        color = scrYellow();
    } else {
        color = "";
    }

    column = (terminalWidth - (int)strlen(message)) / 2 + 1;
    if (column < 1) {
        column = 1;
    }

    printf(SCR_CURSOR, ROW_RESULT, column);
    printf(SCR_ERASE_EOL);
    printf("%s%s%s%s", scrBold(), color, message, scrReset());
    fflush(stdout);
}

void renderMessage(const char *msg, int row, MessageStyle style) {
    const char *prefix;
    const char *suffix;
    const char *displayMessage;
    int terminalWidth;
    int terminalHeight;
    int maxWidth;
    int cut;
    char truncated[512];

    renderInitColors();
    prefix = "";
    suffix = "";

    if (style == MSG_WIN) {
        prefix = scrGreen();
        suffix = scrReset();
    } else if (style == MSG_LOSS) {
        prefix = scrRed();
        suffix = scrReset();
    } else if (style == MSG_PUSH) {
        prefix = scrYellow();
        suffix = scrReset();
    } else if (style == MSG_INFO) {
        prefix = scrDim();
        suffix = scrReset();
    } else if (style == MSG_ACTION) {
        prefix = scrCyan();
        suffix = scrReset();
    } else if (style == MSG_DEALER) {
        prefix = scrMagenta();
        suffix = scrReset();
    }

    terminalGetSize(&terminalWidth, &terminalHeight);

    if (isatty(STDOUT_FILENO) &&
        !renderTerminalSizeOk(&terminalWidth,
                              &terminalHeight)) {
        renderTooSmallScreen(terminalWidth, terminalHeight);
        return;
    }


    if (isatty(STDOUT_FILENO) &&
        terminalWidth >= MIN_TERMINAL_WIDTH &&
        terminalHeight >= MIN_TERMINAL_HEIGHT) {
        displayMessage = msg;
        maxWidth = terminalWidth - messagePadding - 2;

        if (maxWidth < 8) {
            maxWidth = 8;
        }

        if ((int)strlen(msg) > maxWidth) {
            cut = maxWidth - 3;

            if (cut < 1) {
                cut = 1;
            }

            if (cut > (int)sizeof(truncated) - 4) {
                cut = (int)sizeof(truncated) - 4;
            }

            memcpy(truncated, msg, (size_t)cut);
            truncated[cut] = '.';
            truncated[cut + 1] = '.';
            truncated[cut + 2] = '.';
            truncated[cut + 3] = '\0';
            displayMessage = truncated;
        }

        printf(SCR_CURSOR, row, messagePadding + 1);
        printf(SCR_ERASE_EOL);
        printf("%s%s%s", prefix, displayMessage, suffix);
        fflush(stdout);
    } else {
        printf("%s%s%s\n", prefix, msg, suffix);
        fflush(stdout);
    }
}
