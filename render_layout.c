#include "render_layout.h"
#include "render_style.h"
#include "terminal.h"

#include <stdio.h>
#include <string.h>

int getCenteredColumn(int contentWidth) {
    int terminalWidth;
    int terminalHeight;
    int column;

    terminalGetSize(&terminalWidth, &terminalHeight);
    (void)terminalHeight;

    column = (terminalWidth - contentWidth) / 2 + 1;
    if (column < 1) {
        column = 1;
    }

    return column;
}

void printBoxBorder(int row, int column, int width) {
    int index;

    printf(SCR_CURSOR, row, column);
    printf("%s+", scrDim());
    for (index = 0; index < width - 2; index++) {
        putchar('-');
    }
    printf("+%s", scrReset());
}

void printBoxSides(int row, int column, int width) {
    printf(SCR_CURSOR, row, column);
    printf("%s|%s", scrDim(), scrReset());
    printf(SCR_CURSOR, row, column + width - 1);
    printf("%s|%s", scrDim(), scrReset());
}
