#include "render_log.h"
#include "render_control.h"
#include "render_rows.h"
#include "render_style.h"

#include <stdio.h>
#include <string.h>


// Renders the persistent event log from oldest to newest.
void renderEventLog(const Game *game,
                      int padding,
                      int terminalWidth) {
    int rowIndex;
    int sourceIndex;
    int maxWidth;
    int cut;
    const char *prefix;
    const char *suffix;
    const char *displayText;
    char truncated[LOG_LINE_LEN];

    maxWidth = terminalWidth - padding - 2;

    if (maxWidth < 8) {
        maxWidth = 8;
    }

    if (game->log.count == 0) {
        for (rowIndex = 0; rowIndex < LOG_SIZE; rowIndex++) {
            printf(SCR_CURSOR, ROW_LOG_0 + rowIndex, padding + 1);
            printf(SCR_ERASE_EOL);
        }

        fflush(stdout);
        return;
    }

    for (rowIndex = 0; rowIndex < LOG_SIZE; rowIndex++) {
        printf(SCR_CURSOR, ROW_LOG_0 + rowIndex, padding + 1);
        printf(SCR_ERASE_EOL);

        sourceIndex = rowIndex - (LOG_SIZE - game->log.count);

        if (sourceIndex < 0) {
            continue;
        }

        displayText = game->log.lines[sourceIndex];

        if ((int)strlen(displayText) > maxWidth) {
            cut = maxWidth - 3;

            if (cut < 1) {
                cut = 1;
            }

            if (cut > LOG_LINE_LEN - 4) {
                cut = LOG_LINE_LEN - 4;
            }

            memcpy(truncated, displayText, (size_t)cut);
            truncated[cut] = '.';
            truncated[cut + 1] = '.';
            truncated[cut + 2] = '.';
            truncated[cut + 3] = '\0';
            displayText = truncated;
        }

        prefix = "";
        suffix = "";

        if (game->log.styles[sourceIndex] == MSG_WIN) {
            prefix = scrGreen();
            suffix = scrReset();
        } else if (game->log.styles[sourceIndex] == MSG_LOSS) {
            prefix = scrRed();
            suffix = scrReset();
        } else if (game->log.styles[sourceIndex] == MSG_PUSH) {
            prefix = scrYellow();
            suffix = scrReset();
        } else if (game->log.styles[sourceIndex] == MSG_INFO) {
            prefix = scrDim();
            suffix = scrReset();
        } else if (game->log.styles[sourceIndex] == MSG_ACTION) {
            prefix = scrCyan();
            suffix = scrReset();
        } else if (game->log.styles[sourceIndex] == MSG_DEALER) {
            prefix = scrMagenta();
            suffix = scrReset();
        }

        printf("%s%s%s", prefix, displayText, suffix);
    }

    fflush(stdout);
}

