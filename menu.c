#include "menu.h"
#include "render.h"
#include "input.h"
#include "input_map.h"
#include "round.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void dispatchFloorMenuAction(Game *game,
                                    char actionCharacter,
                                    int currentMinBet,
                                    int nextMinBet,
                                    int advanceFee) {
    if ((actionCharacter == 'c' || actionCharacter == 'C') &&
        game->player.chips >= currentMinBet) {
        game->state = STATE_BET;
    } else if ((actionCharacter == 'a' || actionCharacter == 'A') &&
               game->mode == MODE_ROGUELIKE &&
               (long long)game->player.chips >=
                   (long long)nextMinBet + advanceFee) {
        char feeMsg[64];

        game->player.chips -= advanceFee;
        snprintf(feeMsg, sizeof(feeMsg),
                 "Advance cost: -%d chips.", advanceFee);
        logPush(game, feeMsg, MSG_LOSS);
        advanceFloor(game);
        if (game->state != STATE_QUIT) {
            game->state = STATE_BET;
        }
    } else if ((actionCharacter == 's' || actionCharacter == 'S') &&
               game->mode == MODE_ROGUELIKE) {
        game->state = STATE_SHOP;
    } else if (actionCharacter == 'q' || actionCharacter == 'Q') {
        game->state = STATE_QUIT;
    }
}


void floorMenu(Game *game) {
    MenuItem menuItems[4];
    char continueLabel[128];
    char advanceLabel[128];
    char shopLabel[128];
    char input[64];
    KeyEvent event;
    int nextMinBet;
    int advanceFee;
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

    currentMinBet = getMinBet(game, game->run.floor, game->mode);

    if (game->player.chips < currentMinBet) {
        renderMessage("You cannot afford the minimum bet. Run over.",
                      ROW_RESULT,
                      MSG_LOSS);
        game->state = STATE_QUIT;
        return;
    }

    nextMinBet = getMinBet(game, game->run.floor + 1, game->mode);
    advanceFee = game->mode == MODE_ROGUELIKE ? 2 * nextMinBet : 0;
    selectedIndex = 0;

    while (game->state == STATE_MENU) {
        disabledMask = 0;

        snprintf(continueLabel,
                 sizeof(continueLabel),
                 "Continue on floor %d (round %d, fatigue %d%%)",
                 game->run.floor,
                 game->run.roundsOnFloor,
                 fatiguePercent(game->run.roundsOnFloor));
        if (game->mode == MODE_ROGUELIKE) {
            if ((long long)game->player.chips >=
                (long long)advanceFee + nextMinBet) {
                snprintf(advanceLabel, sizeof(advanceLabel),
                         "Advance to floor %d (fee %d + min bet %d)",
                         game->run.floor + 1, advanceFee, nextMinBet);
            } else {
                snprintf(advanceLabel, sizeof(advanceLabel),
                         "Advance to floor %d (need %d total: fee %d + bet %d)",
                         game->run.floor + 1, advanceFee + nextMinBet,
                         advanceFee, nextMinBet);
            }
        } else {
            snprintf(advanceLabel, sizeof(advanceLabel),
                     "Advance to floor %d (earn marks, reset fatigue)",
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
                (long long)game->player.chips <
                    (long long)nextMinBet + advanceFee) {
                disabledMask |= 1 << menuScanIndex;
            }
        }


        if (selectedIndex >= itemCount) {
            selectedIndex = itemCount - 1;
        }

        rawMode = inputIsRaw();

        renderMenu("What's next?",
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
                                        currentMinBet,
                                        nextMinBet,
                                        advanceFee);
                renderMenuLogRefresh(game);
            } else if (event.type == KEY_CHAR) {
                for (menuIndex = 0; menuIndex < itemCount; menuIndex++) {
                    if (event.ch == menuItems[menuIndex].hotkey ||
                        tolower((unsigned char)event.ch) ==
                        menuItems[menuIndex].hotkey) {
                        selectedIndex = menuIndex;
                        dispatchFloorMenuAction(
                            game,
                            menuItems[menuIndex].hotkey,
                            currentMinBet,
                            nextMinBet,
                            advanceFee);
                        renderMenuLogRefresh(game);
                        break;
                    }
                }
            } else if (event.type == KEY_ESC ||
                       event.type == KEY_EOF) {
                game->state = STATE_QUIT;
            }
        } else {
            actionCharacter = -1;

            if (fgets(input, sizeof(input), stdin) == NULL) {
                game->state = STATE_QUIT;
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
                                currentMinBet,
                                nextMinBet,
                                advanceFee);
                            renderMenuLogRefresh(game);
                            break;
                        }
                    }
                }
            }
        }
    }
}

static void purchasePerk(Game *game, int perkIndex) {
    char message[128];

    if (game->player.perkOwned[perkIndex]) {
        logPush(game, "Already owned.", MSG_DEFAULT);
    } else if (game->run.marks < perkCost((PerkId)perkIndex)) {
        logPush(game, "Not enough marks.", MSG_DEFAULT);
    } else {
        game->run.marks -= perkCost((PerkId)perkIndex);
        game->player.perkOwned[perkIndex] = 1;

        if (perkIndex == PERK_PEEK ||
            perkIndex == PERK_SECOND_WIND ||
            perkIndex == PERK_INSURANCE_PERK ||
            perkIndex == PERK_DEALERS_FAVOR ||
            perkIndex == PERK_MARKED_CARD ||
            perkIndex == PERK_MULLIGAN ||
            perkIndex == PERK_EXTRA_LIFE) {
            game->player.perkCharges[perkIndex] = 1;
        }

        snprintf(message,
                 sizeof(message),
                 "%s purchased.",
                 perkName((PerkId)perkIndex));
        logPush(game, message, MSG_WIN);
    }
}

void runShop(Game *game) {
    char input[64];
    char labels[24][128];
    const char *descriptions[24];
    MenuItem items[24];
    PerkId soldPerks[12];
    PerkId itemPerks[24];
    KeyEvent event;
    int actionCharacter;
    int inputCharacter;
    int perkIndex;
    int soldCount;
    int itemCount;
    int selectedIndex;
    int disabledMask;
    int rawMode;
    int characterIndex;
    int menuIndex;
    int itemIndex;
    int nextMinBet;
    int currentMinBet;
    char message[128];

    if (game->mode != MODE_ROGUELIKE) {
        game->state = STATE_MENU;
        return;
    }

    soldCount = 0;
    for (perkIndex = 0; perkIndex < PERK_COUNT; perkIndex++) {
        if (perkIsSold((PerkId)perkIndex)) {
            soldPerks[soldCount++] = (PerkId)perkIndex;
        }
    }

    selectedIndex = 0;

    while (game->state == STATE_SHOP) {
        itemCount = 0;
        disabledMask = 0;
        nextMinBet = getMinBet(game, game->run.floor + 1, game->mode);
        currentMinBet = getMinBet(game, game->run.floor, game->mode);

        for (perkIndex = 0; perkIndex < soldCount; perkIndex++) {
            PerkId perkId;

            perkId = soldPerks[perkIndex];
            snprintf(labels[itemCount],
                     sizeof(labels[itemCount]),
                     "%-15s  %3d marks   %s",
                     perkName(perkId),
                     perkCost(perkId),
                     game->player.perkOwned[perkId] ? "OWNED" : "BUY");
            items[itemCount].hotkey = perkIndex < 9
                ? (char)('1' + perkIndex)
                : (char)('A' + perkIndex - 9);
            items[itemCount].label = labels[itemCount];
            itemPerks[itemCount] = perkId;
            descriptions[itemCount] = perkDescription(perkId);
            if (game->player.perkOwned[perkId] ||
                game->run.marks < perkCost(perkId)) {
                disabledMask |= 1 << itemCount;
            }
            itemCount++;
        }

#define ADD_SHOP_ITEM(key, labelText, descriptionText, disabled) \
        do { \
            items[itemCount].hotkey = (key); \
            snprintf(labels[itemCount], sizeof(labels[itemCount]), "%s", (labelText)); \
            items[itemCount].label = labels[itemCount]; \
            descriptions[itemCount] = (descriptionText); \
            itemPerks[itemCount] = (PerkId)-1; \
            if (disabled) disabledMask |= 1 << itemCount; \
            itemCount++; \
        } while (0)

        ADD_SHOP_ITEM('v', "Convert 10 chips to 2 marks",
                      "Trade 10 chips for 2 marks.",
                      game->player.chips < 10 ||
                      (long long)game->player.chips - 10 < currentMinBet);
        if (game->safetyNetArmed == 0) {
            ADD_SHOP_ITEM('n', "Buy safety net (50 chips)",
                          "Next wipe leaves you with 20 chips.",
                          game->player.chips < 50 ||
                          (long long)game->player.chips - 50 < currentMinBet);
        }
        ADD_SHOP_ITEM('t', "Tip dealer (10 chips)",
                      "Hole card visible next round.",
                      game->player.chips < 10 ||
                      (long long)game->player.chips - 10 < currentMinBet);
        if (game->loanAmount == 0) {
            ADD_SHOP_ITEM('l', "Take a loan (+50 chips, repay 75)",
                          "Repay on the due floor or the run ends.",
                          game->player.chips >= nextMinBet);
        }
        if (game->run.floor >= 4 && game->run.twistRerolled == 0) {
            ADD_SHOP_ITEM('r', "Reroll twist (30 marks)",
                          "Roll a new twist for the current floor.",
                          game->run.marks < 30);
        }
        if (game->run.floor >= 4 && game->run.twistBlocked == 0 &&
            game->run.twistId != TWIST_NONE) {
            ADD_SHOP_ITEM('b', "Block twist (40 marks)",
                          "Neutralize the current twist.",
                          game->run.marks < 40);
        }
#undef ADD_SHOP_ITEM

        if (selectedIndex >= itemCount) {
            selectedIndex = itemCount - 1;
        }

        renderShop(game, items, descriptions, itemCount,
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
                game->state = STATE_MENU;
                continue;
            } else {
                continue;
            }
        } else {
            actionCharacter = -1;
            if (fgets(input, sizeof(input), stdin) == NULL) {
                game->state = STATE_MENU;
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
                game->state = STATE_MENU;
                continue;
            }
        }

        if (actionCharacter == 'q' || actionCharacter == 'Q') {
            game->state = STATE_MENU;
            continue;
        }

        menuIndex = -1;
        /* Prefer exact uppercase perk shortcuts over case-insensitive service keys. */
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
            purchasePerk(game, (int)itemPerks[menuIndex]);
            continue;
        }

        switch (items[menuIndex].hotkey) {
            case 'v':
                game->player.chips -= 10;
                game->run.marks += 2;
                logPush(game, "Converted 10 chips to 2 marks.", MSG_INFO);
                break;
            case 'n':
                game->player.chips -= 50;
                game->safetyNetArmed = 1;
                logPush(game,
                        "Safety net armed. Next wipe leaves 20 chips.",
                        MSG_INFO);
                break;
            case 't':
                game->player.chips -= 10;
                game->tippedThisRound = 1;
                logPush(game,
                        "Dealer tipped. Hole card visible next round.",
                        MSG_INFO);
                break;
            case 'l':
                creditChips(game, 50);
                game->loanAmount = 75;
                game->loanDueFloor = game->run.floor + 3;
                snprintf(message, sizeof(message),
                         "Loan taken: +50 chips, repay 75 by floor %d.",
                         game->loanDueFloor);
                logPush(game, message, MSG_INFO);
                break;
            case 'r':
                game->run.twistId = selectTwistForFloor(game);
                game->run.twistRerolled = 1;
                game->run.marks -= 30;
                logPush(game, "Twist rerolled.", MSG_INFO);
                break;
            case 'b':
                game->run.twistBlocked = 1;
                game->run.marks -= 40;
                logPush(game, "Twist blocked this floor.", MSG_INFO);
                break;
        }
    }
}
