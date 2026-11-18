#define _DEFAULT_SOURCE
#include "menu.h"
#include "render_transitions.h"
#include "render_table.h"
#include "render_menu.h"
#include "render_help.h"
#include "render_shop.h"
#include "render_rows.h"
#include "input.h"
#include "input_map.h"
#include "save.h"
#include "rng.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "round_economy.h"
#include "round_rules.h"

static void dispatchFloorMenuAction(Game *game,
                                    char actionCharacter,
                                    int currentMinBet) {
    FloorMenuActionResult result;

    result = handleFloorMenuAction(game, actionCharacter, currentMinBet);
    if (result == FLOOR_MENU_HANDLED &&
        tolower((unsigned char)actionCharacter) == 'a' &&
        game->state != STATE_QUIT) {
        renderFloorTransition(game);
        renderPause(game, 1200);
        flushInput();
    }

    if (result != FLOOR_MENU_SAVE_REQUESTED) {
        return;
    }

    game->rngCalls = rngGetCallCount();
    if (saveGame(game) == 0) {
        logPush(game, "Game saved.", MSG_INFO);
    } else {
        logPush(game, "Save failed.", MSG_LOSS);
    }
}


void floorMenu(Game *game) {
    MenuItem menuItems[5];
    char continueLabel[128];
    char advanceLabel[128];
    char shopLabel[128];
    char input[64];
    KeyEvent event;
    int disabledMask;
    int selectedIndex;
    int actionCharacter;
    int characterIndex;
    int currentMinBet;
    int inputCharacter;
    int menuIndex;
    int menuScanIndex;
    int rawMode;
    int itemCount;
    AdvanceForecast forecast;

    currentMinBet = getMinBet(game, game->run.floor, game->mode);

    if (game->player.chips < currentMinBet) {
        renderTableMessage("You cannot afford the minimum bet. Run over.",
                      ROW_RESULT,
                      MSG_LOSS);
        game->state = STATE_QUIT;
        return;
    }

    selectedIndex = 0;

    while (game->state == STATE_MENU) {
        disabledMask = 0;

        snprintf(continueLabel,
                 sizeof(continueLabel),
                 "Continue on floor %d (round %d, fatigue %d%%)",
                 game->run.floor,
                 game->run.roundsOnFloor,
                 fatiguePercent(game->run.roundsOnFloor));
        computeAdvanceForecast(game, &forecast);
        if (game->mode == MODE_ROGUELIKE) {
            if (forecast.affordable) {
                if (forecast.loanDue > 0) {
                    snprintf(advanceLabel, sizeof(advanceLabel),
                             "Advance to floor %d (fee %d, min %d, loan %d) -> %d chips",
                             game->run.floor + 1,
                             forecast.fee,
                             forecast.nextMinBet,
                             forecast.loanDue,
                             forecast.postChips);
                } else {
                    snprintf(advanceLabel, sizeof(advanceLabel),
                             "Advance to floor %d (fee %d, min %d) -> %d chips",
                             game->run.floor + 1,
                             forecast.fee,
                             forecast.nextMinBet,
                             forecast.postChips);
                }
            } else {
                snprintf(advanceLabel, sizeof(advanceLabel),
                         "Advance to floor %d (need %d, have %d)",
                         game->run.floor + 1,
                         forecast.fee + forecast.nextMinBet +
                             forecast.loanDue,
                         game->player.chips);
            }
        } else {
            snprintf(advanceLabel, sizeof(advanceLabel),
                     "Advance to floor %d",
                     game->run.floor + 1);
        }
        snprintf(shopLabel,
                 sizeof(shopLabel),
                 "Shop (%d marks)",
                 game->run.marks);
        itemCount = 0;

        menuItems[itemCount].hotkey = 'c';
        menuItems[itemCount].label = continueLabel;
        itemCount++;

        menuItems[itemCount].hotkey = 'a';
        menuItems[itemCount].label = advanceLabel;
        itemCount++;

        menuItems[itemCount].hotkey = 'w';
        menuItems[itemCount].label = "Save game";
        itemCount++;

        menuItems[itemCount].hotkey = 's';
        menuItems[itemCount].label = shopLabel;
        itemCount++;

        menuItems[itemCount].hotkey = 'q';
        menuItems[itemCount].label = "Quit run";
        itemCount++;

        for (menuScanIndex = 0;
             menuScanIndex < itemCount;
             menuScanIndex++) {
            if (menuItems[menuScanIndex].hotkey == 'a' &&
                !forecast.affordable) {
                disabledMask |= 1 << menuScanIndex;
            }
        }


        if (selectedIndex >= itemCount) {
            selectedIndex = itemCount - 1;
        }

        rawMode = inputIsRaw();

        renderMenu("What's next?  (? for help)",
                   menuItems,
                   itemCount,
                   disabledMask,
                   selectedIndex,
                   ROW_MENU_START);
        renderMenuLogRefresh(game);

        if (rawMode) {
            event = readKey();

            if (event.type == KEY_UP) {
                selectedIndex = moveSelection(selectedIndex,
                                              itemCount,
                                              disabledMask,
                                              -1);
            } else if (event.type == KEY_DOWN) {
                selectedIndex = moveSelection(selectedIndex,
                                              itemCount,
                                              disabledMask,
                                              1);
            } else if (event.type == KEY_LEFT ||
                       event.type == KEY_RIGHT) {
            } else if (event.type == KEY_ENTER) {
                actionCharacter = menuItems[selectedIndex].hotkey;
                dispatchFloorMenuAction(game,
                                        actionCharacter,
                                        currentMinBet);
                renderMenuLogRefresh(game);
            } else if (event.type == KEY_CHAR) {
                if (event.ch == '?') {
                    renderHelpOverlay(game);
                    (void)readKey();
                    renderMenuLogRefresh(game);
                    continue;
                }

                for (menuIndex = 0; menuIndex < itemCount; menuIndex++) {
                    if (event.ch == menuItems[menuIndex].hotkey ||
                        tolower((unsigned char)event.ch) ==
                        menuItems[menuIndex].hotkey) {
                        selectedIndex = menuIndex;
                        dispatchFloorMenuAction(
                            game,
                            menuItems[menuIndex].hotkey,
                            currentMinBet);
                        renderMenuLogRefresh(game);
                        break;
                    }
                }
            } else if (event.type == KEY_ESC ||
                       event.type == KEY_EOF) {
                dispatchFloorMenuAction(game, 'q', currentMinBet);
            }
        } else {
            actionCharacter = -1;

            if (fgets(input, sizeof(input), stdin) == NULL) {
                dispatchFloorMenuAction(game, 'q', currentMinBet);
            } else {
                if (strchr(input, '\n') == NULL) {
                    while ((inputCharacter = getchar()) != '\n' &&
                           inputCharacter != EOF) {
                    }
                }

                characterIndex = 0;

                while (input[characterIndex] != '\0') {
                    if (!isspace((unsigned char)input[characterIndex])) {
                        actionCharacter = input[characterIndex];
                        break;
                    }

                    characterIndex++;
                }

                if (actionCharacter != -1) {
                    for (menuIndex = 0; menuIndex < itemCount; menuIndex++) {
                        if (actionCharacter == menuItems[menuIndex].hotkey ||
                            tolower((unsigned char)actionCharacter) ==
                            menuItems[menuIndex].hotkey) {
                            dispatchFloorMenuAction(
                                game,
                                menuItems[menuIndex].hotkey,
                                currentMinBet);
                            renderMenuLogRefresh(game);
                            break;
                        }
                    }
                }
            }
        }
    }
}

void runShop(Game *game) {
    char input[64];
    char labels[24][128];
    const char *descriptions[24];
    MenuItem items[24];
    int itemGroups[24];
    int itemPerks[24]; // -1 marks service entries.
    KeyEvent event;
    int actionCharacter;
    int inputCharacter;
    int perkIndex;
    int itemCount;
    int selectedIndex;
    int disabledMask;
    int rawMode;
    int characterIndex;
    int menuIndex;
    int itemIndex;
    int currentMinBet;
    char costBuf[16];
    char serviceLabels[7][128];
    char offeredServiceIds[7];
    int serviceLabelCount;
    int serviceOfferCount;
    int serviceIndex;
    char serviceId;

    if (handleShopMenuAction(game, 0)) {
        return;
    }

    selectedIndex = 0;

    while (game->state == STATE_SHOP) {
        itemCount = 0;
        disabledMask = 0;
        currentMinBet = getMinBet(game, game->run.floor, game->mode);

        for (perkIndex = 0;
             perkIndex < game->run.shopPerkCount;
             perkIndex++) {
            PerkId perkId;
            const char *status;

            perkId = game->run.shopPerks[perkIndex];
            if (isPerkLockedThisFloor(game, perkId)) {
                status = "LOCKED";
            } else {
                status = game->player.perkOwned[perkId] ? "OWNED" : "BUY";
            }
            snprintf(costBuf, sizeof(costBuf),
                     "%d marks", perkCost(perkId));
            snprintf(labels[itemCount],
                     sizeof(labels[itemCount]),
                     "%-13s  %-9s  %s",
                     perkName(perkId), costBuf, status);
            items[itemCount].hotkey = (char)('1' + perkIndex);
            items[itemCount].label = labels[itemCount];
            itemPerks[itemCount] = perkId;
            itemGroups[itemCount] = 0;
            descriptions[itemCount] = perkDescription(perkId);

            if (game->player.perkOwned[perkId] ||
                isPerkLockedThisFloor(game, perkId) ||
                game->run.marks < perkCost(perkId)) {
                disabledMask |= 1 << itemCount;
            }
            itemCount++;
        }

#define ADD_SHOP_ITEM(key, labelText, descriptionText, disabled, groupValue) \
        do { \
            items[itemCount].hotkey = (key); \
            snprintf(labels[itemCount], sizeof(labels[itemCount]), "%s", (labelText)); \
            items[itemCount].label = labels[itemCount]; \
            descriptions[itemCount] = (descriptionText); \
            itemPerks[itemCount] = -1; \
            itemGroups[itemCount] = (groupValue); \
            if (disabled) disabledMask |= 1 << itemCount; \
            itemCount++; \
        } while (0)

        serviceLabelCount = 0;
        serviceOfferCount = getShopServiceOffers(
            game, offeredServiceIds, 7);
        for (serviceIndex = 0;
             serviceIndex < serviceOfferCount;
             serviceIndex++) {
            serviceId = offeredServiceIds[serviceIndex];

            if (serviceId == 'v') {
                snprintf(serviceLabels[serviceLabelCount],
                         sizeof(serviceLabels[serviceLabelCount]),
                         "%-13s  %-9s  %s",
                         "Convert chips", "10 chips", "-> 2 marks");
                ADD_SHOP_ITEM('v', serviceLabels[serviceLabelCount++],
                              "Trade 10 chips for 2 marks.",
                              !canPurchaseShopService(game, 'v',
                                                      currentMinBet), 1);
            } else if (serviceId == 'n') {
                snprintf(serviceLabels[serviceLabelCount],
                         sizeof(serviceLabels[serviceLabelCount]),
                         "%-13s  %-9s  %s",
                         "Safety net", "50 chips", "survive wipe");
                ADD_SHOP_ITEM('n', serviceLabels[serviceLabelCount++],
                              "Next wipe leaves you with 20 chips.",
                              !canPurchaseShopService(game, 'n',
                                                      currentMinBet), 1);
            } else if (serviceId == 't') {
                snprintf(serviceLabels[serviceLabelCount],
                         sizeof(serviceLabels[serviceLabelCount]),
                         "%-13s  %-9s  %s",
                         "Tip dealer", "10 chips", "hole card next round");
                ADD_SHOP_ITEM('t', serviceLabels[serviceLabelCount++],
                              "Hole card visible next round.",
                              !canPurchaseShopService(game, 't',
                                                      currentMinBet), 1);
            } else if (serviceId == 'z') {
                snprintf(serviceLabels[serviceLabelCount],
                         sizeof(serviceLabels[serviceLabelCount]),
                         "%-13s  %-9s  %s",
                         "Reroll shop", "10 marks", "new perk pool");
                ADD_SHOP_ITEM('z', serviceLabels[serviceLabelCount++],
                              "Roll a new set of perks for this floor.",
                              !canPurchaseShopService(game, 'z',
                                                      currentMinBet), 1);
            } else if (serviceId == 'l') {
                char loanEffect[32];

                snprintf(loanEffect, sizeof(loanEffect),
                         "repay 75 by F%d", game->run.floor + 3);
                snprintf(serviceLabels[serviceLabelCount],
                         sizeof(serviceLabels[serviceLabelCount]),
                         "%-13s  %-9s  %s",
                         "Loan", "+50 chips", loanEffect);
                ADD_SHOP_ITEM('l', serviceLabels[serviceLabelCount++],
                              "Repay on the due floor or the run ends.",
                              !canPurchaseShopService(game, 'l',
                                                      currentMinBet), 1);
            } else if (serviceId == 'r') {
                snprintf(serviceLabels[serviceLabelCount],
                         sizeof(serviceLabels[serviceLabelCount]),
                         "%-13s  %-9s  %s",
                         "Reroll twist", "30 marks", "new twist");
                ADD_SHOP_ITEM('r', serviceLabels[serviceLabelCount++],
                              "Roll a new twist for the current floor.",
                              !canPurchaseShopService(game, 'r',
                                                      currentMinBet), 2);
            } else if (serviceId == 'b') {
                snprintf(serviceLabels[serviceLabelCount],
                         sizeof(serviceLabels[serviceLabelCount]),
                         "%-13s  %-9s  %s",
                         "Block twist", "40 marks", "neutralize");
                ADD_SHOP_ITEM('b', serviceLabels[serviceLabelCount++],
                              "Neutralize the current twist.",
                              !canPurchaseShopService(game, 'b',
                                                      currentMinBet), 2);
            }
        }
#undef ADD_SHOP_ITEM

        if (selectedIndex >= itemCount) {
            selectedIndex = itemCount - 1;
        }

        renderShop(game, items, descriptions, itemGroups, itemCount,
                   disabledMask, selectedIndex);
        rawMode = inputIsRaw();

        if (rawMode) {
            event = readKey();
            if (event.type == KEY_UP) {
                selectedIndex = moveSelection(selectedIndex, itemCount,
                                              disabledMask, -1);
                continue;
            } else if (event.type == KEY_DOWN) {
                selectedIndex = moveSelection(selectedIndex, itemCount,
                                              disabledMask, 1);
                continue;
            } else if (event.type == KEY_ENTER) {
                actionCharacter = items[selectedIndex].hotkey;
            } else if (event.type == KEY_CHAR) {
                actionCharacter = event.ch;
            } else if (event.type == KEY_ESC || event.type == KEY_EOF) {
                (void)handleShopMenuAction(game, event.type == KEY_ESC ? 27 : 0);
                continue;
            } else {
                continue;
            }
        } else {
            actionCharacter = -1;
            if (fgets(input, sizeof(input), stdin) == NULL) {
                (void)handleShopMenuAction(game, 0);
                continue;
            }
            if (strchr(input, '\n') == NULL) {
                while ((inputCharacter = getchar()) != '\n' &&
                       inputCharacter != EOF) {
                }
            }
            characterIndex = 0;
            while (input[characterIndex] != '\0') {
                if (!isspace((unsigned char)input[characterIndex])) {
                    actionCharacter = input[characterIndex];
                    break;
                }
                characterIndex++;
            }
            if (actionCharacter == -1 || actionCharacter == 27) {
                (void)handleShopMenuAction(game, actionCharacter);
                continue;
            }
        }

        if (handleShopMenuAction(game, actionCharacter)) {
            continue;
        }

        menuIndex = -1;
        // Prefer exact uppercase perk shortcuts over case-insensitive service keys.
        for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
            if (actionCharacter == items[itemIndex].hotkey) {
                menuIndex = itemIndex;
                break;
            }
        }
        if (menuIndex < 0) {
            for (itemIndex = 0; itemIndex < itemCount; itemIndex++) {
                if (items[itemIndex].hotkey >= 'a' &&
                    items[itemIndex].hotkey <= 'z' &&
                    tolower((unsigned char)actionCharacter) ==
                    items[itemIndex].hotkey) {
                    menuIndex = itemIndex;
                    break;
                }
            }
        }

        if (menuIndex < 0) {
            continue;
        }
        selectedIndex = menuIndex;
        if ((disabledMask & (1 << menuIndex)) != 0) {
            logPush(game, "Item unavailable.", MSG_DEFAULT);
            continue;
        }

        if (itemPerks[menuIndex] >= 0) {
            if (purchasePerk(game, (int)itemPerks[menuIndex]) &&
                !game->fastMode && isatty(STDOUT_FILENO)) {
                char savedLabel[128];
                int labelLength;

                snprintf(savedLabel, sizeof(savedLabel), "%s",
                         items[menuIndex].label);
                labelLength = (int)strlen(savedLabel);
                if (labelLength > (int)(sizeof("[ PURCHASED ]") - 1)) {
                    memset(labels[menuIndex], ' ', (size_t)labelLength);
                    memcpy(labels[menuIndex], "[ PURCHASED ]",
                           sizeof("[ PURCHASED ]") - 1);
                    labels[menuIndex][labelLength] = '\0';
                } else {
                    snprintf(labels[menuIndex], sizeof(labels[menuIndex]),
                             "[ PURCHASED ]");
                }

                renderShop(game, items, descriptions, itemGroups, itemCount,
                           disabledMask, selectedIndex);
                renderPause(game, 300);
                flushInput();
                snprintf(labels[menuIndex], sizeof(labels[menuIndex]), "%s",
                         savedLabel);
            }
            continue;
        }

        if (purchaseShopService(game,
                               items[menuIndex].hotkey,
                               currentMinBet) &&
            items[menuIndex].hotkey == 'z') {
            selectedIndex = 0;
        }
    }
}
