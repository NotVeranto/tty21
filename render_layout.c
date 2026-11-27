#include "render_layout.h"
#include "render_control.h"
#include "render_style.h"
#include "terminal.h"

#include <stdio.h>
#include <string.h>

/* SGR sequence for a palette entry (empty when colour is off). Small ring
 * of buffers so several can be used inside one printf. */
static const char *styleSeq(RenderStyle style) {
    static char ring[6][64];
    static int next;
    char *slot;

    slot = ring[next];
    next = (next + 1) % 6;
    renderStyleSgr(style, slot, sizeof(ring[0]));
    return slot;
}

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
    printf("%s+", styleSeq(ST_BRASS));
    for (index = 0; index < width - 2; index++) {
        putchar('-');
    }
    printf("+%s", scrReset());
}

void printBoxSides(int row, int column, int width) {
    printf(SCR_CURSOR, row, column);
    printf("%s|%s", styleSeq(ST_BRASS), scrReset());
    printf(SCR_CURSOR, row, column + width - 1);
    printf("%s|%s", styleSeq(ST_BRASS), scrReset());
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
    printf("%s%s%s", styleSeq(ST_GOLD_B), title, scrReset());

    if (subtitle != NULL) {
        printf(SCR_CURSOR, row + 1, getCenteredColumn((int)strlen(subtitle)));
        printf("%s%s%s", styleSeq(ST_MUTED), subtitle, scrReset());
    }

    separatorWidth = terminalWidth < 60 ? terminalWidth : 60;
    if (separatorWidth < 1) {
        separatorWidth = 1;
    }

    printf(SCR_CURSOR,
           row + (subtitle != NULL ? 2 : 1),
           getCenteredColumn(separatorWidth));
    printf("%s", styleSeq(ST_BRASS));
    while (separatorWidth-- > 0) {
        fputs(renderUtf8Enabled() ? "\xe2\x94\x80" : "-", stdout);
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
               styleSeq(ST_MUTED), hotkey, scrReset(),
               styleSeq(ST_MUTED), label, scrReset());
    } else if (completed) {
        printf("  %s[%c]%s  %s%s%s",
               styleSeq(ST_MUTED), hotkey, scrReset(),
               styleSeq(ST_OK), label, scrReset());
    } else if (selected) {
        printf("%s> %s[%c]%s  %s%s%s",
               styleSeq(ST_GOLD_B), styleSeq(ST_GOLD_B), hotkey, scrReset(),
               styleSeq(ST_STRONG), label, scrReset());
    } else {
        printf("  %s[%c]%s  %s",
               styleSeq(ST_BRASS), hotkey, scrReset(), label);
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
