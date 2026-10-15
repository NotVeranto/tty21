#include "menu.h"
#include "render.h"
#include "input.h"
#include "input_map.h"
#include "round.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int allPerksOwned(const Game *game) {
    int perkIndex;

    for (perkIndex = 0; perkIndex < PERK_COUNT; perkIndex++) {
        if (!game->player.perkOwned[perkIndex]) {
            return 0;
        }
    }

    return 1;
}

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
               game->player.chips >= nextMinBet &&
               game->player.chips >= advanceFee) {
        char feeMsg[64];

        game->player.chips -= advanceFee;
        snprintf(feeMsg, sizeof(feeMsg),
                 "Advance cost: -%d chips.", advanceFee);
        logPush(game, feeMsg, MSG_LOSS);
        advanceFloor(game);
        if (game->state != STATE_QUIT) {
            game->state = STATE_BET;
        }
    } else if ((actionCharacter == 'a' || actionCharacter == 'A') &&
               game->mode == MODE_CLASSIC &&
               game->player.chips >= nextMinBet) {
        advanceFloor(game);
        if (game->state != STATE_QUIT) {
            game->state = STATE_BET;
        }
    } else if (actionCharacter == 's' || actionCharacter == 'S') {
        game->state = STATE_SHOP;
    } else if ((actionCharacter == 'm' || actionCharacter == 'M') &&
               game->mode == MODE_ROGUELIKE &&
               game->player.chips >= 10) {
        char convertMsg[96];

        game->player.chips -= 10;
        game->run.marks += 2;

        /* Flat conversion rate: 10 chips always buys 2 marks. */
        snprintf(convertMsg, sizeof(convertMsg),
                 "Converted 10 chips to 2 marks.");
        logPush(game, convertMsg, MSG_INFO);
    } else if ((actionCharacter == 'l' || actionCharacter == 'L') &&
               game->loanAmount == 0 &&
               game->player.chips < nextMinBet) {
        char message[128];

        game->player.chips += 50;
        game->loanAmount = 75;
        game->loanDueFloor = game->run.floor + 3;
        snprintf(message,
                 sizeof(message),
                 "Loan taken: +50 chips, repay 75 by floor %d.",
                 game->loanDueFloor);
        logPush(game, message, MSG_INFO);
    } else if ((actionCharacter == 'r' || actionCharacter == 'R') &&
               game->mode == MODE_ROGUELIKE &&
               game->run.floor >= 4 &&
               game->run.twistRerolled == 0 &&
               game->run.marks >= 30) {
        game->run.twistId = selectTwistForFloor(game);
        game->run.twistRerolled = 1;
        game->run.marks -= 30;
        logPush(game, "Twist rerolled.", MSG_INFO);
    } else if ((actionCharacter == 'b' || actionCharacter == 'B') &&
               game->mode == MODE_ROGUELIKE &&
               game->run.floor >= 4 &&
               game->run.twistBlocked == 0 &&
               game->run.twistId != TWIST_NONE &&
               game->run.marks >= 40) {
        game->run.twistBlocked = 1;
        game->run.marks -= 40;
        logPush(game, "Twist blocked this floor.", MSG_INFO);
    } else if ((actionCharacter == 'k' || actionCharacter == 'K') &&
               game->mode == MODE_ROGUELIKE &&
               game->run.floor >= 4 &&
               game->player.chips >= nextMinBet &&
               game->run.marks >= 60) {
        game->run.marks -= 60;
        advanceFloor(game);
        if (game->state != STATE_QUIT) {
            game->state = STATE_BET;
            logPush(game, "Floor skipped.", MSG_INFO);
        }
    } else if ((actionCharacter == 'p' || actionCharacter == 'P') &&
               game->extraPerkSlots == 0 &&
               game->run.marks >= 100) {
        game->run.marks -= 100;
        game->extraPerkSlots = 1;
        logPush(game, "Extra perk slot unlocked.", MSG_WIN);
    } else if (actionCharacter == 'q' || actionCharacter == 'Q') {
        game->state = STATE_QUIT;
    }
}

void floorMenu(Game *game) {
    MenuItem menuItems[10];
    char continueLabel[128];
    char advanceLabel[128];
    char shopLabel[128];
    char convertLabel[128];
    char loanLabel[128];
    char rerollLabel[128];
    char blockLabel[128];
    char skipLabel[128];
    char perkSlotLabel[128];
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
    int loanAvailable;

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
            if (game->player.chips >= advanceFee) {
                snprintf(advanceLabel, sizeof(advanceLabel),
                         "Advance to floor %d (cost %d chips)",
                         game->run.floor + 1, advanceFee);
            } else {
                snprintf(advanceLabel, sizeof(advanceLabel),
                         "Advance to floor %d (need %d chips)",
                         game->run.floor + 1, advanceFee);
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
        snprintf(convertLabel, sizeof(convertLabel),
                 "Convert 10 chips to 2 marks");
        snprintf(loanLabel,
                 sizeof(loanLabel),
                 "Take a loan (50 chips, repay 75 by floor %d)",
                 game->run.floor + 3);
        snprintf(rerollLabel,
                 sizeof(rerollLabel),
                 "Reroll twist (30 marks)");
        snprintf(blockLabel,
                 sizeof(blockLabel),
                 "Block twist (40 marks)");
        snprintf(skipLabel,
                 sizeof(skipLabel),
                 "Skip floor (60 marks)");
        snprintf(perkSlotLabel,
                 sizeof(perkSlotLabel),
                 "Buy extra perk slot (100 marks)");

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

        if (game->mode == MODE_ROGUELIKE) {
            menuItems[itemCount].hotkey = 'm';
            menuItems[itemCount].label = convertLabel;
            itemCount++;
        }

        loanAvailable = game->loanAmount == 0 &&
                        game->player.chips < nextMinBet;

        if (loanAvailable) {
            menuItems[itemCount].hotkey = 'l';
            menuItems[itemCount].label = loanLabel;
            itemCount++;
        }

        if (game->mode == MODE_ROGUELIKE &&
            game->run.floor >= 4 &&
            game->run.twistRerolled == 0 &&
            game->run.marks >= 30) {
            menuItems[itemCount].hotkey = 'r';
            menuItems[itemCount].label = rerollLabel;
            itemCount++;
        }

        if (game->mode == MODE_ROGUELIKE &&
            game->run.floor >= 4 &&
            game->run.twistBlocked == 0 &&
            game->run.twistId != TWIST_NONE &&
            game->run.marks >= 40) {
            menuItems[itemCount].hotkey = 'b';
            menuItems[itemCount].label = blockLabel;
            itemCount++;
        }

        if (game->mode == MODE_ROGUELIKE &&
            game->run.floor >= 4 &&
            game->player.chips >= nextMinBet &&
            game->run.marks >= 60) {
            menuItems[itemCount].hotkey = 'k';
            menuItems[itemCount].label = skipLabel;
            itemCount++;
        }

        if (game->extraPerkSlots == 0 &&
            game->run.marks >= 100) {
            menuItems[itemCount].hotkey = 'p';
            menuItems[itemCount].label = perkSlotLabel;
            itemCount++;
        }

        menuItems[itemCount].hotkey = 'q';
        menuItems[itemCount].label = "Quit run";
        itemCount++;

        for (menuScanIndex = 0;
             menuScanIndex < itemCount;
             menuScanIndex++) {
            if (menuItems[menuScanIndex].hotkey == 'a' &&
                (game->player.chips < nextMinBet ||
                 game->player.chips < advanceFee)) {
                disabledMask |= 1 << menuScanIndex;
            }
            if (menuItems[menuScanIndex].hotkey == 'm' &&
                game->player.chips < 10) {
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
    char labels[PERK_COUNT][128];
    MenuItem items[PERK_COUNT];
    KeyEvent event;
    int actionCharacter;
    int inputCharacter;
    int perkIndex;
    int selectedIndex;
    int ownedMask;
    int rawMode;
    int characterIndex;
    int menuIndex;

    selectedIndex = 0;

    while (game->state == STATE_SHOP) {
        for (perkIndex = 0; perkIndex < PERK_COUNT; perkIndex++) {
            const char *status;

            status = game->player.perkOwned[perkIndex] ? "OWNED" : "BUY";
            snprintf(labels[perkIndex],
                     sizeof(labels[perkIndex]),
                     "%-15s  %3d marks   %s",
                     perkName((PerkId)perkIndex),
                     perkCost((PerkId)perkIndex),
                     status);

            if (perkIndex < 9) {
                items[perkIndex].hotkey = (char)('1' + perkIndex);
            } else {
                items[perkIndex].hotkey = (char)('A' + perkIndex - 9);
            }
            items[perkIndex].label = labels[perkIndex];
        }

        ownedMask = 0;

        for (perkIndex = 0; perkIndex < PERK_COUNT; perkIndex++) {
            if (game->player.perkOwned[perkIndex]) {
                ownedMask |= 1 << perkIndex;
            }
        }

        renderShop(game, items, selectedIndex);

        rawMode = inputIsRaw();

        if (rawMode) {
            event = readKey();

            if (event.type == KEY_UP) {
                selectedIndex = moveSelection(selectedIndex,
                                              PERK_COUNT,
                                              ownedMask,
                                              -1);
            } else if (event.type == KEY_DOWN) {
                selectedIndex = moveSelection(selectedIndex,
                                              PERK_COUNT,
                                              ownedMask,
                                              1);
            } else if (event.type == KEY_LEFT ||
                       event.type == KEY_RIGHT) {
            } else if (event.type == KEY_ENTER) {
                if (!allPerksOwned(game)) {
                    purchasePerk(game, selectedIndex);
                }
            } else if (event.type == KEY_CHAR) {
                actionCharacter = event.ch;

                if (actionCharacter == 'q' ||
                    actionCharacter == 'Q') {
                    game->state = STATE_MENU;
                } else {
                    menuIndex = -1;

                    for (perkIndex = 0; perkIndex < PERK_COUNT; perkIndex++) {
                        if (actionCharacter == items[perkIndex].hotkey) {
                            menuIndex = perkIndex;
                            break;
                        }
                    }

                    if (menuIndex >= 0) {
                        selectedIndex = menuIndex;
                        purchasePerk(game, selectedIndex);
                    }
                }
            } else if (event.type == KEY_ESC ||
                       event.type == KEY_EOF) {
                game->state = STATE_MENU;
            }
        } else {
            actionCharacter = -1;

            if (fgets(input, sizeof(input), stdin) == NULL) {
                game->state = STATE_MENU;
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

                if (actionCharacter == -1 ||
                    actionCharacter == 27) {
                    game->state = STATE_MENU;
                } else if (actionCharacter == 'q' ||
                           actionCharacter == 'Q') {
                    game->state = STATE_MENU;
                } else {
                    menuIndex = -1;

                    for (perkIndex = 0;
                         perkIndex < PERK_COUNT;
                         perkIndex++) {
                        if (actionCharacter == items[perkIndex].hotkey) {
                            menuIndex = perkIndex;
                            break;
                        }
                    }

                    if (menuIndex >= 0) {
                        selectedIndex = menuIndex;
                        purchasePerk(game, selectedIndex);
                    }
                }
            }
        }
    }
}
