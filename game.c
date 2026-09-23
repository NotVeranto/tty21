#include "game.h"
#include "render.h"
#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Keeps a safety margin before reshuffling at the start of a round.
#define DECK_RESHUFFLE_MARGIN 15
#define GAME_PROMPT_ROW 29
#define GAME_RESULT_ROW 31
#define GAME_CHIPS_ROW 32
#define GAME_OVER_ROW 34
#define GAME_FINAL_CHIPS_ROW 35

// Draws the next card and advances the deck position.
static void drawCard(Deck *deck, Hand *hand) {
    if (deck->top >= DECK_SIZE) {
        initDeck(deck);
        shuffleDeck(deck);
    }

    addCardToHand(hand, &deck->cards[deck->top]);
    deck->top++;
}

// Bet input stays line-based so fgets can handle multi-character numeric input and backspace.
// Returns -1 on EOF so quitting is distinct from a valid bet.
static int readBet(int chips) {
    char input[64];
    char prompt[64];
    char *endPointer;
    long betValue;
    size_t newlinePosition;
    int validBet;
    int inputCharacter;

    validBet = 0;
    snprintf(prompt, sizeof(prompt), "Enter bet (1-%d): ", chips);

    while (!validBet) {
        renderMessage(prompt, GAME_PROMPT_ROW);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            return -1;
        }

        newlinePosition = strcspn(input, "\n");

        if (input[newlinePosition] != '\n') {
            while ((inputCharacter = getchar()) != '\n' && inputCharacter != EOF) {
            }

            if (inputCharacter == EOF) {
                return -1;
            }

            snprintf(prompt,
                     sizeof(prompt),
                     "Invalid bet. Enter an integer between 1 and %d.",
                     chips);
            renderMessage(prompt, GAME_RESULT_ROW);
            snprintf(prompt, sizeof(prompt), "Enter bet (1-%d): ", chips);
            continue;
        }

        input[newlinePosition] = '\0';
        betValue = strtol(input, &endPointer, 10);

        while (*endPointer == ' ' || *endPointer == '\t') {
            endPointer++;
        }

        if (input[0] != '\0' &&
            *endPointer == '\0' &&
            betValue >= 1 &&
            betValue <= chips) {
            validBet = 1;
        } else {
            snprintf(prompt,
                     sizeof(prompt),
                     "Invalid bet. Enter an integer between 1 and %d.",
                     chips);
            renderMessage(prompt, GAME_RESULT_ROW);
            snprintf(prompt, sizeof(prompt), "Enter bet (1-%d): ", chips);
        }
    }

    return (int)betValue;
}

void initGame(Game *game) {
    initDeck(&game->deck);
    shuffleDeck(&game->deck);
    resetHand(&game->player.hand);
    resetHand(&game->dealer.hand);
    game->player.chips = 100;
    game->player.bet = 0;
    game->state = STATE_BET;
    game->roundNumber = 0;
}

void startRound(Game *game) {
    int rawMode;

    if (game->deck.top > DECK_SIZE - DECK_RESHUFFLE_MARGIN) {
        initDeck(&game->deck);
        shuffleDeck(&game->deck);
    }

    renderTable(game, 0);

    rawMode = inputIsRaw();
    if (rawMode) {
        disableRawMode();
    }

    game->player.bet = readBet(game->player.chips);

    if (rawMode && game->player.bet >= 0) {
        inputInit();
    }

    if (game->player.bet < 0) {
        game->state = STATE_QUIT;
    } else {
        resetHand(&game->player.hand);
        resetHand(&game->dealer.hand);
        game->roundNumber++;

        drawCard(&game->deck, &game->player.hand);
        drawCard(&game->deck, &game->dealer.hand);
        drawCard(&game->deck, &game->player.hand);
        drawCard(&game->deck, &game->dealer.hand);

        if (game->player.hand.value == 21) {
            game->state = STATE_RESULT;
        } else {
            game->state = STATE_PLAYER_TURN;
        }
    }
}

void playerTurn(Game *game) {
    int inputCharacter;
    int actionCharacter;

    if (inputIsRaw()) {
        renderTable(game, 1);

        while (game->state == STATE_PLAYER_TURN) {
            renderMessage("Hit or stand? [h/s]: ", GAME_PROMPT_ROW);
            actionCharacter = getch();

            if (actionCharacter == -1) {
                game->state = STATE_QUIT;
                break;
            }

            if (actionCharacter == 'h' || actionCharacter == 'H') {
                drawCard(&game->deck, &game->player.hand);

                if (game->player.hand.value > 21) {
                    game->state = STATE_RESULT;
                }

                renderTable(game, game->state == STATE_PLAYER_TURN);
            } else if (actionCharacter == 's' || actionCharacter == 'S') {
                game->state = STATE_DEALER_TURN;
                renderTable(game, 0);
            } else if (actionCharacter == 'q' || actionCharacter == 'Q') {
                game->state = STATE_QUIT;
            }
        }
    } else {
        while (game->state == STATE_PLAYER_TURN) {
            renderTable(game, 1);
            renderMessage("Hit or stand? [h/s]: ", GAME_PROMPT_ROW);
            actionCharacter = getchar();

            if (actionCharacter == EOF) {
                game->state = STATE_QUIT;
                break;
            }

            while ((inputCharacter = getchar()) != '\n' && inputCharacter != EOF) {
            }

            if (actionCharacter == 'h' || actionCharacter == 'H') {
                drawCard(&game->deck, &game->player.hand);

                if (game->player.hand.value > 21) {
                    game->state = STATE_RESULT;
                }
            } else if (actionCharacter == 's' || actionCharacter == 'S') {
                game->state = STATE_DEALER_TURN;
            } else {
                renderMessage("Invalid action. Enter h to hit or s to stand.",
                              GAME_PROMPT_ROW);
            }
        }
    }
}

void dealerTurn(Game *game) {
    if (game->state == STATE_DEALER_TURN) {
        while (game->dealer.hand.value < 17) {
            drawCard(&game->deck, &game->dealer.hand);
        }

        game->state = STATE_RESULT;
    }
}

void resolveRound(Game *game) {
    int playerValue;
    int dealerValue;
    int playerBlackjack;
    int dealerBlackjack;
    int payout;
    char resultMessage[128];

    playerValue = game->player.hand.value;
    dealerValue = game->dealer.hand.value;
    playerBlackjack = game->player.hand.count == 2 && playerValue == 21;
    dealerBlackjack = game->dealer.hand.count == 2 && dealerValue == 21;
    payout = 0;

    if (playerValue > 21) {
        game->player.chips -= game->player.bet;
        snprintf(resultMessage,
                 sizeof(resultMessage),
                 "Player busts. You lose %d chips.",
                 game->player.bet);
    } else if (playerBlackjack && !dealerBlackjack) {
        payout = game->player.bet * 3 / 2;
        game->player.chips += payout;
        snprintf(resultMessage,
                 sizeof(resultMessage),
                 "Blackjack! You win %d chips.",
                 payout);
    } else if (dealerBlackjack && !playerBlackjack) {
        game->player.chips -= game->player.bet;
        snprintf(resultMessage,
                 sizeof(resultMessage),
                 "Dealer has blackjack. You lose %d chips.",
                 game->player.bet);
    } else if (playerValue == dealerValue) {
        snprintf(resultMessage,
                 sizeof(resultMessage),
                 "Push. Your bet is returned.");
    } else if (dealerValue > 21 || playerValue > dealerValue) {
        game->player.chips += game->player.bet;
        snprintf(resultMessage,
                 sizeof(resultMessage),
                 "You win %d chips.",
                 game->player.bet);
    } else {
        game->player.chips -= game->player.bet;
        snprintf(resultMessage,
                 sizeof(resultMessage),
                 "You lose %d chips.",
                 game->player.bet);
    }

    renderTable(game, 0);
    renderMessage(resultMessage, GAME_RESULT_ROW);
    snprintf(resultMessage,
             sizeof(resultMessage),
             "Chips: %d",
             game->player.chips);
    renderMessage(resultMessage, GAME_CHIPS_ROW);

    if (game->player.chips == 0) {
        game->state = STATE_QUIT;
    } else {
        game->state = STATE_BET;
    }
}

void printGameState(const Game *game, int hideDealerCard) {
    int cardIndex;

    printf("Round: %d\n", game->roundNumber);
    printf("Dealer: ");

    for (cardIndex = 0; cardIndex < game->dealer.hand.count; cardIndex++) {
        if (hideDealerCard && cardIndex == 0) {
            printf("??");
        } else {
            printCard(&game->dealer.hand.cards[cardIndex]);
        }

        if (cardIndex < game->dealer.hand.count - 1) {
            printf(" ");
        }
    }

    if (hideDealerCard && game->dealer.hand.count > 0) {
        printf("\n");
    } else {
        printf(" | Total value: %d\n", game->dealer.hand.value);
    }

    printf("Player: ");
    printHand(&game->player.hand);
    printf("Chips: %d | Bet: %d\n", game->player.chips, game->player.bet);
}

int runGame(Game *game) {
    char finalChipsMessage[64];

    while (game->state != STATE_QUIT) {
        if (game->state == STATE_BET) {
            startRound(game);
        } else if (game->state == STATE_PLAYER_TURN) {
            playerTurn(game);
        } else if (game->state == STATE_DEALER_TURN) {
            dealerTurn(game);
        } else if (game->state == STATE_RESULT) {
            resolveRound(game);
        }
    }

    renderMessage("Game over.", GAME_OVER_ROW);
    snprintf(finalChipsMessage,
             sizeof(finalChipsMessage),
             "Final chips: %d",
             game->player.chips);
    renderMessage(finalChipsMessage, GAME_FINAL_CHIPS_ROW);

    return 0;
}
