#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Keeps a safety margin before reshuffling at the start of a round.
#define DECK_RESHUFFLE_MARGIN 15

// Draws the next card and advances the deck position.
static void drawCard(Deck *deck, Hand *hand) {
    if (deck->top >= DECK_SIZE) {
        initDeck(deck);
        shuffleDeck(deck);
    }

    addCardToHand(hand, &deck->cards[deck->top]);
    deck->top++;
}

// Returns -1 on EOF so quitting is distinct from a valid bet.
static int readBet(int chips) {
    char input[64];
    char *endPointer;
    long betValue;
    size_t newlinePosition;
    int validBet;
    int inputCharacter;

    validBet = 0;

    while (!validBet) {
        printf("Enter bet (1-%d): ", chips);

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

            printf("Invalid bet. Enter an integer between 1 and %d.\n", chips);
            continue;
        }

        input[newlinePosition] = '\0';
        betValue = strtol(input, &endPointer, 10);

        while (*endPointer == ' ' || *endPointer == '\t') {
            endPointer++;
        }

        if (input[0] != '\0' && *endPointer == '\0' && betValue >= 1 && betValue <= chips) {
            validBet = 1;
        } else {
            printf("Invalid bet. Enter an integer between 1 and %d.\n", chips);
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
    if (game->deck.top > DECK_SIZE - DECK_RESHUFFLE_MARGIN) {
        initDeck(&game->deck);
        shuffleDeck(&game->deck);
    }

    game->player.bet = readBet(game->player.chips);

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

    while (game->state == STATE_PLAYER_TURN) {
        printGameState(game, 1);
        printf("Hit or stand? [h/s]: ");
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
            printf("Invalid action. Enter h to hit or s to stand.\n");
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
    int playerValue, dealerValue;
    int playerBlackjack, dealerBlackjack;
    int payout;

    playerValue = game->player.hand.value;
    dealerValue = game->dealer.hand.value;
    playerBlackjack = game->player.hand.count == 2 && playerValue == 21;
    dealerBlackjack = game->dealer.hand.count == 2 && dealerValue == 21;
    payout = 0;

    printGameState(game, 0);

    if (playerValue > 21) {
        game->player.chips -= game->player.bet;
        printf("Player busts. You lose %d chips.\n", game->player.bet);
    } else if (playerBlackjack && !dealerBlackjack) {
        payout = game->player.bet * 3 / 2;
        game->player.chips += payout;
        printf("Blackjack! You win %d chips.\n", payout);
    } else if (dealerBlackjack && !playerBlackjack) {
        game->player.chips -= game->player.bet;
        printf("Dealer has blackjack. You lose %d chips.\n", game->player.bet);
    } else if (playerValue == dealerValue) {
        printf("Push. Your bet is returned.\n");
    } else if (dealerValue > 21 || playerValue > dealerValue) {
        game->player.chips += game->player.bet;
        printf("You win %d chips.\n", game->player.bet);
    } else {
        game->player.chips -= game->player.bet;
        printf("You lose %d chips.\n", game->player.bet);
    }

    printf("Chips: %d\n", game->player.chips);

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
    while (game->state != STATE_QUIT) {
        if (game->state == STATE_BET) {
            if (game->roundNumber > 0) {
                printf("--------------------\n");
            }
            startRound(game);
        } else if (game->state == STATE_PLAYER_TURN) {
            playerTurn(game);
        } else if (game->state == STATE_DEALER_TURN) {
            dealerTurn(game);
        } else if (game->state == STATE_RESULT) {
            resolveRound(game);
        }
    }

    printf("Game over. Final chips: %d\n", game->player.chips);
    return 0;
}
