#ifndef RENDER_SHOP_H
#define RENDER_SHOP_H

#include "game.h"
#include "render_menu.h"

void renderShop(const Game *game,
                const MenuItem *items,
                const char **descriptions,
                const int *groups,
                int itemCount,
                int disabledMask,
                int selectedIndex);

#endif
