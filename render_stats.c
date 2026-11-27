#include "render_stats.h"
#include "render_control.h"
#include "render_style.h"
#include "render_layout.h"
#include "render_terminal.h"
#include "render_canvas.h"
#include "render_scene.h"
#include "terminal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define STATS_BOX_W 40
#define STATS_BOX_H 11

static void drawStatRow(int x, int y, int width, const char *label,
                        int value) {
    char number[24];
    int numberW;
    int labelW;

    snprintf(number, sizeof(number), "%d", value);
    numberW = (int)strlen(number);
    labelW = canvasPutText(x, y, label, ST_MUTED);
    canvasHLine(x + labelW + 1, y, width - labelW - numberW - 2,
                renderGlyphs()->dot, ST_BRASS);
    canvasPutText(x + width - numberW, y, number, ST_GOLD_B);
}

static void renderStatsCanvas(const Game *game, int width, int height) {
    const Glyphs *glyphs;
    const char *title;
    int boxX;
    int boxY;
    int innerX;
    int innerW;

    glyphs = renderGlyphs();
    title = game->mode == MODE_CLASSIC ? "CLASSIC SUMMARY"
                                       : "RUN SUMMARY";
    renderHideCursor();
    boxX = (width - STATS_BOX_W) / 2 + 1;
    boxY = (height - STATS_BOX_H) / 2 + 1;
    if (boxX < 1) boxX = 1;
    if (boxY < 1) boxY = 1;

    canvasBlank();
    renderBackdrop(1, 1, width, height);
    canvasBox(boxX, boxY, STATS_BOX_W, STATS_BOX_H, title,
              ST_BRASS, ST_PLAIN, ST_GOLD_B);
    innerX = boxX + 3;
    innerW = STATS_BOX_W - 6;
    if (game->mode == MODE_CLASSIC) {
        drawStatRow(innerX, boxY + 3, innerW, "Rounds Played",
                    game->roundNumber);
        drawStatRow(innerX, boxY + 4, innerW, "Starting Chips", 100);
        drawStatRow(innerX, boxY + 5, innerW, "Final Chips",
                    game->player.chips);
        drawStatRow(innerX, boxY + 6, innerW, "Net Result",
                    game->player.chips - 100);
    } else {
        drawStatRow(innerX, boxY + 3, innerW, "Floors Reached",
                    game->totalFloorsReached);
        drawStatRow(innerX, boxY + 4, innerW, "Bosses Defeated",
                    game->totalBossesDefeated);
        drawStatRow(innerX, boxY + 5, innerW, "Total Marks",
                    game->totalMarksEarned);
    }
    canvasHLine(innerX, boxY + 7, innerW, glyphs->rule, ST_BRASS);
    canvasPutCentered(boxX, STATS_BOX_W, boxY + 9, "Press any key to return",
                      ST_MUTED);
    canvasInvalidate();
    canvasFlush();
}

void renderStatsScreen(const Game *game) {
    int width;
    int height;
    int startRow;
    int padding;
    int boxWidth;
    int i;
    char lines[9][80];

    terminalGetSize(&width, &height);
    if (isatty(STDOUT_FILENO) &&
        !renderTerminalSizeOk(&width, &height)) {
        renderTooSmallScreen(width, height);
        return;
    }

    if (isatty(STDOUT_FILENO) && canvasBegin()) {
        renderStatsCanvas(game, width, height);
        return;
    }

    snprintf(lines[0], sizeof(lines[0]), "+------------------------------------+");
    if (game->mode == MODE_CLASSIC) {
        snprintf(lines[1], sizeof(lines[1]), "|           CLASSIC SUMMARY          |");
        snprintf(lines[2], sizeof(lines[2]), "|------------------------------------|");
        snprintf(lines[3], sizeof(lines[3]), "| Rounds Played:    %-16d |",
                 game->roundNumber);
        snprintf(lines[4], sizeof(lines[4]), "| Starting Chips:   %-16d |", 100);
        snprintf(lines[5], sizeof(lines[5]), "| Final Chips:      %-16d |",
                 game->player.chips);
        snprintf(lines[6], sizeof(lines[6]), "| Net Result:       %-16d |",
                 game->player.chips - 100);
        snprintf(lines[7], sizeof(lines[7]), "|       Press any key to return      |");
        snprintf(lines[8], sizeof(lines[8]), "+------------------------------------+");
    } else {
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
    }

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
