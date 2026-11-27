#include "render_shop.h"
#include "render_canvas.h"
#include "render_control.h"
#include "render_rows.h"
#include "render_scene.h"
#include "render_terminal.h"
#include "render_layout.h"
#include "render_cards.h"
#include "render_style.h"
#include "render_log.h"
#include "terminal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SHOP_ITEM_START_ROW 7
#define SHOP_ITEM_ROWS 15
#define SHOP_FRAME_ROWS 22
#define SHOP_DESCRIPTION_ROW 24
#define SHOP_HINT_ROW 25
#define SHOP_KEYS_HINT \
    "Keys: 1-9/A-C perks; V convert; N net; T tip; L loan; R/B twists; P slot; Q close"

static const char *GROUP_TITLES[] = {
    "PERKS", "SERVICES", "TWIST TOOLS"
};

typedef struct {
    int first;        /* first visible display line */
    int selectedLine;
    int totalLines;
} Scroll;

static int isPurchasedLabel(const char *label) {
    return strncmp(label, "[ PURCHASED ]", sizeof("[ PURCHASED ]") - 1) == 0;
}

static int groupHasTitle(int group) {
    return group >= 0 && group < 3;
}

static void measure(const int *groups, int itemCount, int selectedIndex,
                    Scroll *scroll) {
    int itemIndex;
    int prevGroup;
    int line;

    line = 0;
    prevGroup = -1;
    scroll->selectedLine = 0;
    for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
        if (groups[itemIndex] != prevGroup) {
            if (groupHasTitle(groups[itemIndex])) {
                line++;
            }
            prevGroup = groups[itemIndex];
        }
        if (itemIndex == selectedIndex) {
            scroll->selectedLine = line;
        }
        line++;
    }
    scroll->totalLines = line;
    scroll->first = scroll->selectedLine - SHOP_ITEM_ROWS / 2;
    if (scroll->first > scroll->totalLines - SHOP_ITEM_ROWS) {
        scroll->first = scroll->totalLines - SHOP_ITEM_ROWS;
    }
    if (scroll->first < 0) {
        scroll->first = 0;
    }
    /* keep the heading of the selected group visible when possible */
    if (scroll->selectedLine == scroll->first && scroll->first > 0) {
        scroll->first--;
    }
}

static void drawItem(const MenuItem *item, int selected, int disabled,
                     int purchased, int row, int innerX, int innerW,
                     const Glyphs *glyphs) {
    char key[8];

    snprintf(key, sizeof(key), "[%c]", item->hotkey);
    if (selected && !disabled) {
        canvasFill(innerX, row, innerW, 1, ST_SEL);
        canvasPutText(innerX + 1, row, glyphs->pointer, ST_SEL);
        canvasPutText(innerX + 3, row, key, ST_SEL_KEY);
        canvasPutFit(innerX + 8, row, innerW - 9, item->label, ST_SEL);
    } else if (disabled) {
        canvasPutText(innerX + 3, row, key, ST_MUTED);
        canvasPutFit(innerX + 8, row, innerW - 9, item->label, ST_MUTED);
    } else if (purchased) {
        canvasPutText(innerX + 1, row, glyphs->check, ST_OK);
        canvasPutText(innerX + 3, row, key, ST_MUTED);
        canvasPutFit(innerX + 8, row, innerW - 9, item->label, ST_OK);
    } else {
        canvasPutText(innerX + 3, row, key, ST_GOLD);
        canvasPutFit(innerX + 8, row, innerW - 9, item->label, ST_PLAIN);
    }
}

void renderShop(const Game *game,
                const MenuItem *items,
                const char **descriptions,
                const int *groups,
                int itemCount,
                int disabledMask,
                int selectedIndex) {
    const Glyphs *glyphs;
    TableLayout layout;
    Scroll scroll;
    char text[96];
    int terminalWidth;
    int terminalHeight;
    int itemIndex;
    int prevGroup;
    int line;
    int row;
    int innerX;
    int innerW;
    int isDisabled;
    int isPurchased;
    int groupIndex;

    renderInitColors();
    terminalGetSize(&terminalWidth, &terminalHeight);
    if (isatty(STDOUT_FILENO) &&
        !renderTerminalSizeOk(&terminalWidth, &terminalHeight)) {
        renderTooSmallScreen(terminalWidth, terminalHeight);
        return;
    }

    if (isatty(STDOUT_FILENO) &&
        terminalWidth >= MIN_TERMINAL_WIDTH &&
        terminalHeight >= MIN_TERMINAL_HEIGHT &&
        canvasBegin()) {
        glyphs = renderGlyphs();
        renderHideCursor();

        memset(&layout, 0, sizeof(layout));
        layout.termW = terminalWidth;
        layout.termH = terminalHeight;
        layout.fw = terminalWidth < 76 ? terminalWidth : 76;
        layout.fl = (terminalWidth - layout.fw) / 2 + 1;
        layout.innerL = layout.fl + 1;
        layout.innerR = layout.fl + layout.fw - 2;
        renderLayoutSetCurrent(&layout);

        canvasBlank();
        canvasBox(layout.fl, 1, layout.fw, SHOP_FRAME_ROWS, "TTY21 - SHOP",
                  ST_BRASS, ST_PLAIN, ST_GOLD_B);
        innerX = layout.innerL + 1;
        innerW = layout.fw - 4;

        canvasPutText(innerX + 1, ROW_SHOP_MARKS, "MARKS", ST_MUTED);
        snprintf(text, sizeof(text), "%d", game->run.marks);
        canvasPutText(innerX + 8, ROW_SHOP_MARKS, text, ST_ACCENT);
        canvasPutText(innerX + 1, ROW_SHOP_CHIPS, "CHIPS", ST_MUTED);
        snprintf(text, sizeof(text), "%d", game->player.chips);
        canvasPutText(innerX + 8, ROW_SHOP_CHIPS, text, ST_GOLD_B);
        canvasHLine(innerX, SHOP_ITEM_START_ROW - 1, innerW,
                    glyphs->rule, ST_BRASS);

        measure(groups, itemCount, selectedIndex, &scroll);
        line = 0;
        prevGroup = -1;
        for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
            if (groups[itemIndex] != prevGroup) {
                groupIndex = groups[itemIndex];
                if (groupHasTitle(groupIndex)) {
                    row = SHOP_ITEM_START_ROW + line - scroll.first;
                    if (line >= scroll.first &&
                        line < scroll.first + SHOP_ITEM_ROWS) {
                        int used;

                        used = canvasPutText(innerX + 1, row,
                                             glyphs->rule, ST_BRASS);
                        used += canvasPutText(innerX + 1 + used, row, " ",
                                              ST_PLAIN);
                        used += canvasPutText(innerX + 1 + used, row,
                                              GROUP_TITLES[groupIndex],
                                              ST_GOLD);
                        canvasPutText(innerX + 1 + used, row, " ",
                                      ST_PLAIN);
                        used++;
                        canvasHLine(innerX + 1 + used, row,
                                    innerW - 2 - used, glyphs->rule,
                                    ST_BRASS);
                    }
                    line++;
                }
                prevGroup = groups[itemIndex];
            }
            if (line >= scroll.first &&
                line < scroll.first + SHOP_ITEM_ROWS) {
                isDisabled = (disabledMask & (1 << itemIndex)) != 0;
                isPurchased = isPurchasedLabel(items[itemIndex].label);
                drawItem(&items[itemIndex], itemIndex == selectedIndex,
                         isDisabled, isPurchased,
                         SHOP_ITEM_START_ROW + line - scroll.first, innerX,
                         innerW, glyphs);
            }
            line++;
        }
        if (scroll.first > 0) {
            canvasPutGlyph(layout.innerR - 1, SHOP_ITEM_START_ROW,
                           glyphs->arrowUp, ST_GOLD);
        }
        if (scroll.first + SHOP_ITEM_ROWS < scroll.totalLines) {
            canvasPutGlyph(layout.innerR - 1,
                           SHOP_ITEM_START_ROW + SHOP_ITEM_ROWS - 1,
                           glyphs->arrowDown, ST_GOLD);
        }

        if (selectedIndex >= 0 && selectedIndex < itemCount &&
            descriptions[selectedIndex] != NULL) {
            canvasPutGlyph(layout.fl, SHOP_DESCRIPTION_ROW, glyphs->bullet,
                           ST_GOLD_B);
            canvasPutFit(layout.fl + 2, SHOP_DESCRIPTION_ROW,
                         layout.fw - 2, descriptions[selectedIndex],
                         ST_STRONG);
        }
        canvasPutFit(layout.fl, SHOP_HINT_ROW, layout.fw, SHOP_KEYS_HINT,
                     ST_MUTED);
        renderLogRows(game, layout.fl - 1, terminalWidth);

        canvasInvalidate();
        canvasFlush();
    } else {
        {
            char header[128];

            formatPlainScreenHeader(header, sizeof(header),
                                    terminalWidth, "TTY21 - SHOP");
            printf("%s\n", header);
        }
        printf("Marks: %d | Chips: %d\n\n", game->run.marks, game->player.chips);
        prevGroup = -1;
        for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
            if (groups[itemIndex] != prevGroup) {
                groupIndex = groups[itemIndex];
                if (groupHasTitle(groupIndex)) {
                    printf("\n%s\n", GROUP_TITLES[groupIndex]);
                }
                prevGroup = groups[itemIndex];
            }
            isDisabled = (disabledMask & (1 << itemIndex)) != 0;
            {
                char plain[512];

                formatPlainMenuItem(plain,
                                    sizeof(plain),
                                    items[itemIndex].hotkey,
                                    items[itemIndex].label,
                                    itemIndex == selectedIndex,
                                    isDisabled);
                printf("%s\n", plain);
            }
            if (itemIndex == selectedIndex && descriptions[itemIndex] != NULL) {
                printf("    %s\n", descriptions[itemIndex]);
            }
        }
        printf("%s\n", SHOP_KEYS_HINT);
        fflush(stdout);
    }
}
