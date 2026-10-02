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
                                    int nextMinBet) {

    if ((actionCharacter == 'c' || actionCharacter == 'C') &&
        game->player.chips >= currentMinBet) {
        game->state = STATE_BET;
    } else if ((actionCharacter == 'a' || actionCharacter == 'A') &&
               game->player.chips >= nextMinBet) {
        advanceFloor(game);
        game->state = STATE_BET;
    } else if (actionCharacter == 's' || actionCharacter == 'S') {
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
    int disabledMask;
    int selectedIndex;
    int actionCharacter;
    int characterIndex;
    int currentMinBet;
    int inputCharacter;
    int menuIndex;
    int rawMode;

    currentMinBet = getMinBet(game->run.floor, game->mode);

    if (game->player.chips < currentMinBet) {
        renderMessage("You cannot afford the minimum bet. Run over.",
                      ROW_RESULT,
                      MSG_LOSS);
        game->state = STATE_QUIT;
        return;
    }

    nextMinBet = getMinBet(game->run.floor + 1, game->mode);
    disabledMask = 0;

    if (game->player.chips < nextMinBet) {
        disabledMask |= 1 << 1;
    }

    snprintf(continueLabel,
             sizeof(continueLabel),
             "Continue playing on floor %d",
             game->run.floor);
    snprintf(advanceLabel,
             sizeof(advanceLabel),
             "Advance to floor %d (earn marks)",
             game->run.floor + 1);
    snprintf(shopLabel,
             sizeof(shopLabel),
             "Shop (%d marks)",
             game->run.marks);

    menuItems[0].hotkey = 'c';
    menuItems[0].label = continueLabel;
    menuItems[1].hotkey = 'a';
    menuItems[1].label = advanceLabel;
    menuItems[2].hotkey = 's';
    menuItems[2].label = shopLabel;
    menuItems[3].hotkey = 'q';
    menuItems[3].label = "Quit run";

    selectedIndex = 0;

    while (game->state == STATE_MENU) {
        rawMode = inputIsRaw();

        renderMenu("What's next?",
                   menuItems,
                   4,
                   disabledMask,
                   selectedIndex,
                   ROW_MENU_START);

        if (rawMode) {
            event = readKey();

            if (event.type == KEY_UP) {
                selectedIndex = moveSelection(selectedIndex,
                                              4,
                                              disabledMask,
                                              -1);
            } else if (event.type == KEY_DOWN) {
                selectedIndex = moveSelection(selectedIndex,
                                              4,
                                              disabledMask,
                                              1);
            } else if (event.type == KEY_LEFT ||
                       event.type == KEY_RIGHT) {
            } else if (event.type == KEY_ENTER) {
                actionCharacter = menuItems[selectedIndex].hotkey;
                dispatchFloorMenuAction(game,
                                        actionCharacter,
                                        currentMinBet,
                                        nextMinBet);
            } else if (event.type == KEY_CHAR) {
                for (menuIndex = 0; menuIndex < 4; menuIndex++) {
                    if (event.ch == menuItems[menuIndex].hotkey ||
                        tolower((unsigned char)event.ch) ==
                        menuItems[menuIndex].hotkey) {
                        selectedIndex = menuIndex;
                        dispatchFloorMenuAction(
                            game,
                            menuItems[menuIndex].hotkey,
                            currentMinBet,
                            nextMinBet);
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
                    for (menuIndex = 0; menuIndex < 4; menuIndex++) {
                        if (actionCharacter == menuItems[menuIndex].hotkey ||
                            tolower((unsigned char)actionCharacter) ==
                            menuItems[menuIndex].hotkey) {
                            dispatchFloorMenuAction(
                                game,
                                menuItems[menuIndex].hotkey,
                                currentMinBet,
                                nextMinBet);
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
            perkIndex == PERK_SECOND_WIND) {
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
                     "%-15s  %3d souls   %s",
                     perkName((PerkId)perkIndex),
                     perkCost((PerkId)perkIndex),
                     status);

            items[perkIndex].hotkey = (char)('1' + perkIndex);
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
