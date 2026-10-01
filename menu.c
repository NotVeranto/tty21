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
    int inputCharacter;

    if ((actionCharacter == 'c' || actionCharacter == 'C') &&
        game->player.chips >= currentMinBet) {
        game->state = STATE_BET;
    } else if ((actionCharacter == 'a' || actionCharacter == 'A') &&
               game->player.chips >= nextMinBet) {
        advanceFloor(game);
        renderMessage("Press any key to continue.",
                      ROW_PROMPT,
                      MSG_DEFAULT);

        if (inputIsRaw()) {
            inputCharacter = getch();
        } else {
            inputCharacter = getchar();

            if (inputCharacter != EOF) {
                while ((inputCharacter = getchar()) != '\n' &&
                       inputCharacter != EOF) {
                }
            }
        }

        if (inputCharacter == -1 || inputCharacter == EOF) {
            game->state = STATE_QUIT;
        } else {
            game->state = STATE_BET;
        }
    } else if (actionCharacter == 's' || actionCharacter == 'S') {
        game->state = STATE_SHOP;
    } else if (actionCharacter == 'q' || actionCharacter == 'Q') {
        game->state = STATE_QUIT;
    }
}

void floorMenu(Game *game) {
    const char *menuItems[4];
    char continueItem[128];
    char advanceItem[128];
    char shopItem[128];
    char input[64];
    KeyEvent event;
    int nextMinBet;
    int disabledMask;
    int selectedIndex;
    int actionCharacter;
    int characterIndex;
    int hasCharacter;
    int currentMinBet;
    int inputCharacter;
    int rawMode;

    currentMinBet = getMinBet(game->run.floor);

    if (game->player.chips < currentMinBet) {
        renderMessage("You cannot afford the minimum bet. Run over.",
                      ROW_RESULT,
                      MSG_LOSS);
        game->state = STATE_QUIT;
        return;
    }

    nextMinBet = getMinBet(game->run.floor + 1);
    disabledMask = 0;

    if (game->player.chips < nextMinBet) {
        disabledMask |= 1 << 1;
    }

    snprintf(continueItem,
             sizeof(continueItem),
             "[C]ontinue playing on floor %d",
             game->run.floor);
    snprintf(advanceItem,
             sizeof(advanceItem),
             "[A]dvance to floor %d (earn marks)",
             game->run.floor + 1);
    snprintf(shopItem,
             sizeof(shopItem),
             "[S]hop (%d marks)",
             game->run.marks);

    menuItems[0] = continueItem;
    menuItems[1] = advanceItem;
    menuItems[2] = shopItem;
    menuItems[3] = "[Q]uit run";

    selectedIndex = 0;
    rawMode = inputIsRaw();

    if (!rawMode) {
        renderMenu("What's next?",
                   menuItems,
                   4,
                   disabledMask,
                   selectedIndex,
                   ROW_MENU_START);
    }

    while (game->state == STATE_MENU) {
        if (rawMode) {
            renderMenu("What's next?",
                       menuItems,
                       4,
                       disabledMask,
                       selectedIndex,
                       ROW_MENU_START);

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
                actionCharacter = "casq"[selectedIndex];
                dispatchFloorMenuAction(game,
                                        (char)actionCharacter,
                                        currentMinBet,
                                        nextMinBet);
            } else if (event.type == KEY_CHAR) {
                actionCharacter = event.ch;

                if (actionCharacter == 'c' || actionCharacter == 'C') {
                    selectedIndex = 0;
                } else if (actionCharacter == 'a' || actionCharacter == 'A') {
                    selectedIndex = 1;
                } else if (actionCharacter == 's' || actionCharacter == 'S') {
                    selectedIndex = 2;
                } else if (actionCharacter == 'q' || actionCharacter == 'Q') {
                    selectedIndex = 3;
                }

                dispatchFloorMenuAction(game,
                                        (char)actionCharacter,
                                        currentMinBet,
                                        nextMinBet);
            } else if (event.type == KEY_ESC ||
                       event.type == KEY_EOF) {
                game->state = STATE_QUIT;
            }
        } else {
            actionCharacter = -1;

            if (fgets(input, sizeof(input), stdin) != NULL) {
                if (strchr(input, '\n') == NULL) {
                    while ((inputCharacter = getchar()) != '\n' &&
                           inputCharacter != EOF) {
                    }
                }

                characterIndex = 0;
                hasCharacter = 0;

                while (input[characterIndex] != '\0') {
                    if (!isspace((unsigned char)input[characterIndex])) {
                        actionCharacter = input[characterIndex];
                        hasCharacter = 1;
                        break;
                    }

                    characterIndex++;
                }

                if (!hasCharacter) {
                    actionCharacter = -1;
                }
            }

            if (actionCharacter == -1) {
                game->state = STATE_QUIT;
            } else {
                dispatchFloorMenuAction(game,
                                        (char)actionCharacter,
                                        currentMinBet,
                                        nextMinBet);
            }
        }
    }
}

void runShop(Game *game) {
    char input[64];
    KeyEvent event;
    int actionCharacter;
    int inputCharacter;
    int perkIndex;
    int selectedIndex;
    int ownedMask;
    int rawMode;
    int characterIndex;

    const char *pendingMessage = NULL;

    selectedIndex = 0;
    rawMode = inputIsRaw();

    while (game->state == STATE_SHOP) {
        ownedMask = 0;

        for (perkIndex = 0; perkIndex < PERK_COUNT; perkIndex++) {
            if (game->player.perkOwned[perkIndex]) {
                ownedMask |= 1 << perkIndex;
            }
        }

        renderShop(game, selectedIndex);

        if (pendingMessage != NULL) {
            renderMessage(pendingMessage, ROW_RESULT, MSG_DEFAULT);
        }

        if (rawMode) {
            event = readKey();

            if (event.type == KEY_UP) {
                selectedIndex = moveSelection(selectedIndex,
                                              PERK_COUNT,
                                              ownedMask,
                                              -1);
                pendingMessage = NULL;
            } else if (event.type == KEY_DOWN) {
                selectedIndex = moveSelection(selectedIndex,
                                              PERK_COUNT,
                                              ownedMask,
                                              1);
                pendingMessage = NULL;
            } else if (event.type == KEY_LEFT ||
                       event.type == KEY_RIGHT) {
                pendingMessage = NULL;
            } else if (event.type == KEY_ENTER) {
                if (allPerksOwned(game)) {
                    pendingMessage = NULL;
                } else if (game->player.perkOwned[selectedIndex]) {
                    pendingMessage = "Already owned.";
                } else if (game->run.marks < perkCost((PerkId)selectedIndex)) {
                    pendingMessage = "Not enough marks.";
                } else {
                    game->run.marks -= perkCost((PerkId)selectedIndex);
                    game->player.perkOwned[selectedIndex] = 1;

                    if (selectedIndex == PERK_PEEK ||
                        selectedIndex == PERK_SECOND_WIND) {
                        game->player.perkCharges[selectedIndex] = 1;
                    }

                    pendingMessage = NULL;
                }
            } else if (event.type == KEY_CHAR) {
                actionCharacter = event.ch;

                if (actionCharacter >= '1' &&
                    actionCharacter <= '5') {
                    selectedIndex = actionCharacter - '1';

                    if (game->player.perkOwned[selectedIndex]) {
                        pendingMessage = "Already owned.";
                    } else if (game->run.marks <
                               perkCost((PerkId)selectedIndex)) {
                        pendingMessage = "Not enough marks.";
                    } else {
                        game->run.marks -=
                            perkCost((PerkId)selectedIndex);
                        game->player.perkOwned[selectedIndex] = 1;

                        if (selectedIndex == PERK_PEEK ||
                            selectedIndex == PERK_SECOND_WIND) {
                            game->player.perkCharges[selectedIndex] = 1;
                        }

                        pendingMessage = NULL;
                    }
                } else if (actionCharacter == 'q' ||
                           actionCharacter == 'Q') {
                    pendingMessage = NULL;
                    game->state = STATE_MENU;
                } else {
                    pendingMessage = NULL;
                }
            } else if (event.type == KEY_ESC ||
                       event.type == KEY_EOF) {
                pendingMessage = NULL;
                game->state = STATE_MENU;
            }
        } else {
            actionCharacter = -1;

            if (fgets(input, sizeof(input), stdin) != NULL) {
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
            }

            if (actionCharacter == -1) {
                pendingMessage = NULL;
                game->state = STATE_MENU;
            } else if (actionCharacter == 'q' ||
                       actionCharacter == 'Q' ||
                       actionCharacter == 27) {
                pendingMessage = NULL;
                game->state = STATE_MENU;
            } else if (actionCharacter >= '1' &&
                       actionCharacter <= '5') {
                selectedIndex = actionCharacter - '1';

                if (game->player.perkOwned[selectedIndex]) {
                    pendingMessage = "Already owned.";
                } else if (game->run.marks <
                           perkCost((PerkId)selectedIndex)) {
                    pendingMessage = "Not enough marks.";
                } else {
                    game->run.marks -= perkCost((PerkId)selectedIndex);
                    game->player.perkOwned[selectedIndex] = 1;

                    if (selectedIndex == PERK_PEEK ||
                        selectedIndex == PERK_SECOND_WIND) {
                        game->player.perkCharges[selectedIndex] = 1;
                    }

                    pendingMessage = NULL;
                }
            } else {
                pendingMessage = NULL;
            }
        }
    }
}
