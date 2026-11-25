#include "render_stats.h"
#include "render_control.h"
#include "render_style.h"
#include "render_layout.h"
#include "terminal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

void renderStatsScreen(const Game *game) {
    int width;
    int height;
    int startRow;
    int padding;
    int boxWidth;
    int i;
    char lines[9][80];

    snprintf(lines[0], sizeof(lines[0]), "+------------------------------------+");
    snprintf(lines[1], sizeof(lines[1]), "|             RUN SUMMARY            |");
    snprintf(lines[2], sizeof(lines[2]), "|------------------------------------|");
    snprintf(lines[3], sizeof(lines[3]), "| Floors Reached:   %-16d |",
             game->totalFloorsReached);
    snprintf(lines[4], sizeof(lines[4]), "| Bosses Defeated:  %-16d |",
             game->totalBossesDefeated);
    snprintf(lines[5], sizeof(lines[5]), "| Total Marks:      %-16d |",
             game->totalMarksEarned);
    snprintf(lines[6], sizeof(lines[6]), "+------------------------------------+");
    snprintf(lines[7], sizeof(lines[7]), "|       Press any key to return      |");
    snprintf(lines[8], sizeof(lines[8]), "+------------------------------------+");

    terminalGetSize(&width, &height);
    if (isatty(STDOUT_FILENO)) {
        printf("%s%s", SCR_CLEAR, SCR_HOME);
    }
    startRow = height > 0 ? (height - 9) / 2 + 1 : 1;
    if (startRow < 1) {
        startRow = 1;
    }
    boxWidth = (int)strlen(lines[0]);
    padding = getCenteredColumnForWidth(width, boxWidth) - 1;

    for (i = 0; i < 9; i++) {
        if (isatty(STDOUT_FILENO)) {
            printf(SCR_CURSOR, startRow + i, padding + 1);
        } else {
            printf("%*s", padding, "");
        }
        printf("%s\n", lines[i]);
    }
    fflush(stdout);
}
