#include "render_layout.h"
#include "render_control.h"
#include "render_style.h"
#include "terminal.h"

#include <stdio.h>
#include <string.h>

int formatPlainScreenHeader(char *buffer,
                            size_t bufferSize,
                            int terminalWidth,
                            const char *title) {
    int column;

    if (buffer == NULL || bufferSize == 0 || title == NULL) {
        return -1;
    }

    column = getCenteredColumnForWidth(terminalWidth, (int)strlen(title));
    return snprintf(buffer, bufferSize, "%*s%s", column - 1, "", title);
}

int getCenteredColumnForWidth(int terminalWidth, int contentWidth) {
    int column = (terminalWidth - contentWidth) / 2 + 1;

    return column < 1 ? 1 : column;
}

int getCenteredColumn(int contentWidth) {
    int terminalWidth;
    int terminalHeight;

    terminalGetSize(&terminalWidth, &terminalHeight);
    (void)terminalHeight;

    return getCenteredColumnForWidth(terminalWidth, contentWidth);
}

void printBoxBorder(int row, int column, int width) {
    int index;

    printf(SCR_CURSOR, row, column);
    printf("%s+", scrMuted());
    for (index = 0; index < width - 2; index++) {
        putchar('-');
    }
    printf("+%s", scrReset());
}

void printBoxSides(int row, int column, int width) {
    printf(SCR_CURSOR, row, column);
    printf("%s|%s", scrMuted(), scrReset());
    printf(SCR_CURSOR, row, column + width - 1);
    printf("%s|%s", scrMuted(), scrReset());
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
    printf("%s%s%s", scrEmphasis(), title, scrReset());

    if (subtitle != NULL) {
        printf(SCR_CURSOR, row + 1, getCenteredColumn((int)strlen(subtitle)));
        printf("%s%s%s", scrMuted(), subtitle, scrReset());
    }

    separatorWidth = terminalWidth < 60 ? terminalWidth : 60;
    if (separatorWidth < 1) {
        separatorWidth = 1;
    }

    printf(SCR_CURSOR,
           row + (subtitle != NULL ? 2 : 1),
           getCenteredColumn(separatorWidth));
    printf("%s", scrMuted());
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
               scrMuted(), hotkey, scrReset(),
               scrMuted(), label, scrReset());
    } else if (completed) {
        printf("  %s[%c]%s  %s%s%s",
               scrMuted(), hotkey, scrReset(),
               scrSuccess(), label, scrReset());
    } else if (selected) {
        printf("%s> %s[%c]%s  %s%s%s",
               scrEmphasis(), scrAccent(), hotkey, scrReset(),
               scrEmphasis(), label, scrReset());
    } else {
        printf("  %s[%c]%s  %s",
               scrMuted(), hotkey, scrReset(), label);
    }
}

/* Formats a menu entry consistently for redirected and non-ANSI output. */
int formatPlainMenuItem(char *buffer,
                        size_t bufferSize,
                        char hotkey,
                        const char *label,
                        int selected,
                        int disabled) {
    if (buffer == NULL || bufferSize == 0 || label == NULL) {
        return -1;
    }

    return snprintf(buffer,
                    bufferSize,
                    "%s[%c]  %s%s",
                    selected ? "> " : "  ",
                    hotkey,
                    label,
                    disabled ? " (disabled)" : "");
}
