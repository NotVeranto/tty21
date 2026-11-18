#ifndef RENDER_MENU_H
#define RENDER_MENU_H

typedef struct {
    char hotkey;
    const char *label;
} MenuItem;

void renderMenu(const char *title,
                const MenuItem *items,
                int itemCount,
                int disabledMask,
                int selectedIndex,
                int startRow);

#endif
