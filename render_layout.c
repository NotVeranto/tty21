#include "render_layout.h"
#include "render_control.h"
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

void renderScreenHeader(const char *title,
                        const char *subtitle,
                        int row) {
    int terminalWidth;
    int terminalHeight;
    int separatorWidth;
    int titleColumn;

    terminalGetSize(&terminalWidth, &terminalHeight);
    (void)terminalHeight;

    titleColumn = getCenteredColumn((int)strlen(title));
    printf(SCR_CURSOR, row, titleColumn);
    printf("%s%s%s", scrBold(), title, scrReset());

    if (subtitle != NULL) {
        printf(SCR_CURSOR, row + 1, getCenteredColumn((int)strlen(subtitle)));
        printf("%s%s%s", scrDim(), subtitle, scrReset());
    }

    separatorWidth = terminalWidth < 60 ? terminalWidth : 60;
    if (separatorWidth < 1) {
        separatorWidth = 1;
    }

    printf(SCR_CURSOR,
           row + (subtitle != NULL ? 2 : 1),
           getCenteredColumn(separatorWidth));
    printf("%s", scrDim());
    while (separatorWidth-- > 0) {
        putchar('-');
    }
    printf("%s", scrReset());
}

/* Draws a menu entry with shared selection and availability styling. */
void renderMenuItemRow(int row,
                       int column,
                       char hotkey,
                       const char *label,
                       int selected,
                       int disabled,
                       int completed) {
    printf(SCR_CURSOR, row, column);
    printf(SCR_ERASE_EOL);

    if (disabled) {
        printf("  %s[%c]%s  %s%s%s",
               scrDim(), hotkey, scrReset(),
               scrDim(), label, scrReset());
    } else if (completed) {
        printf("  %s[%c]%s  %s%s%s",
               scrDim(), hotkey, scrReset(),
               scrGreen(), label, scrReset());
    } else if (selected) {
        printf("%s> %s[%c]%s  %s%s%s",
               scrBold(), scrCyan(), hotkey, scrReset(),
               scrBold(), label, scrReset());
    } else {
        printf("  %s[%c]%s  %s",
               scrDim(), hotkey, scrReset(), label);
    }
}
