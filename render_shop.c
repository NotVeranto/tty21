#include "render.h"
#include "render_layout.h"
#include "render_style.h"
#include "render_log.h"
#include "terminal.h"

#include <stdio.h>
#include <string.h>

#define SHOP_ITEM_START_ROW 7
#define SHOP_DESCRIPTION_ROW 24
#define SHOP_HINT_ROW 25

static void moveToTableRow(int row, int padding) {
    printf(SCR_CURSOR, row, padding + 1);
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
