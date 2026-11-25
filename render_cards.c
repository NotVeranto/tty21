#include "render_cards.h"
#include "render_control.h"
#include "render_style.h"
#include "round_cards.h"
#include <stdio.h>
#include <string.h>

#define CARD_CONTENT_WIDTH (CARD_WIDTH - 2)
#define CARD_HEIGHT 7
#define DEALER_CARD_ROW 5
#define PLAYER_CARD_ROW 14

static const char *CARD_RANKS[] = {
    "", "A", "2", "3", "4", "5", "6",
    "7", "8", "9", "10", "J", "Q", "K"
};
static const char *CARD_SUITS[] = { "♥", "♦", "♣", "♠" };

int getDynamicTableWidth(const Game *game) {
    int cardsTotal;
    int playerCardsTotal;
    int handIndex;
    int dealerHandIndex;
    int totalDealerCards;

    totalDealerCards = 0;
    for (dealerHandIndex = 0;
         dealerHandIndex < game->dealer.handCount;
         dealerHandIndex++) {
        totalDealerCards += game->dealer.hands[dealerHandIndex].count;
    }
    if (game->dealer.handCount > 1) {
        totalDealerCards += (game->dealer.handCount - 1) * 2;
    }

    cardsTotal = totalDealerCards;
    playerCardsTotal = 0;
    for (handIndex = 0;
         handIndex < game->player.handCount;
         handIndex++) {
        playerCardsTotal += game->player.hands[handIndex].count;
    }
    if (game->player.handCount > 1) {
        playerCardsTotal += (game->player.handCount - 1) * 2;
    }
    if (playerCardsTotal > cardsTotal) {
        cardsTotal = playerCardsTotal;
    }
    if (cardsTotal < 5) {
        cardsTotal = 5;
    }
    return cardsTotal * CARD_WIDTH + (cardsTotal - 1) * CARD_GAP;
}

static void moveToTableRow(int row, int padding) {
    printf(SCR_CURSOR, row, padding + 1);
}

static const char *curseColor(CurseId id) {
    switch (id) {
        case CURSED_BLEED: return scrRed();
        case CURSED_FROST: return scrCyan();
        case CURSED_ECHO:  return scrYellow();
        default: return "";
    }
}

static void printFaceDownLine(int lineIndex) {
    static const char *PATTERN_LINES[] = {
        " . . . . ",
        " ####### ",
        " ## # ## ",
        " ####### ",
        " . . . . "
    };

    if (lineIndex < 0 ||
        lineIndex >= (int)(sizeof(PATTERN_LINES) / sizeof(PATTERN_LINES[0]))) {
        return;
    }

    printf("|%s|", PATTERN_LINES[lineIndex]);
}

static void printVisibleCardLine(const Card *card, int lineIndex) {
    const char *rankName;
    const char *ink;

    rankName = CARD_RANKS[card->rank];
    ink = card->suit == HEARTS || card->suit == DIAMONDS
              ? scrRed() : scrDim();

    if (lineIndex == 0) {
        printf("|%s%-2s%s", ink, rankName, scrReset());
        if (card->cursed != CURSED_NONE) {
            printf("      %s%s*%s|",
                   curseColor((CurseId)card->cursed),
                   scrBold(), scrReset());
        } else {
            printf("       |");
        }
    } else if (lineIndex == 1 || lineIndex == 3) {
        printf("|         |");
    } else if (lineIndex == 2) {
        printf("|    %s%s%s    |", ink, CARD_SUITS[card->suit],
               scrReset());
    } else {
        printf("|       %s%2s%s|", ink, rankName, scrReset());
    }

}
static void drawCards(const Hand *hand,
                      int startRow,
                      int padding,
                      int hideFirstCard) {
    int lineIndex;
    int cardIndex;

    for (lineIndex = 0; lineIndex < CARD_HEIGHT; lineIndex++) {
        moveToTableRow(startRow + lineIndex, padding);
        for (cardIndex = 0; cardIndex < hand->count; cardIndex++) {
            if (lineIndex == 0 || lineIndex == CARD_HEIGHT - 1) {
                printf("+---------+");
            } else if (hideFirstCard && cardIndex == 0) {
                printFaceDownLine(lineIndex - 1);
            } else {
                printVisibleCardLine(&hand->cards[cardIndex], lineIndex - 1);
            }
            if (cardIndex < hand->count - 1) {
                printf(" ");
            }
        }
    }
}

void renderDrawDealerHands(const Game *game,
                           int padding,
                           int hideFirstCard) {
    int handIndex;
    int handPadding;
    int handWidth;

    if (game->dealer.handCount <= 0) {
        return;
    }
    handPadding = padding;
    if (game->dealer.handCount > 1) {
        for (handIndex = 0;
             handIndex < game->dealer.handCount;
             handIndex++) {
            moveToTableRow(DEALER_CARD_ROW - 1, handPadding);
            printf("%sHAND %d%s", scrDim(), handIndex + 1, scrReset());
            handWidth = game->dealer.hands[handIndex].count * CARD_WIDTH +
                        (game->dealer.hands[handIndex].count - 1) * CARD_GAP;
            handPadding += handWidth + 2;
        }
    }

    handPadding = padding;
    for (handIndex = 0;
         handIndex < game->dealer.handCount;
         handIndex++) {
        drawCards(&game->dealer.hands[handIndex],
                  DEALER_CARD_ROW, handPadding, hideFirstCard);
        handWidth = game->dealer.hands[handIndex].count * CARD_WIDTH +
                    (game->dealer.hands[handIndex].count - 1) * CARD_GAP;
        handPadding += handWidth + 2;
    }
}

static void printPlayerHandStatus(const Game *game,
                                  int handIndex,
                                  int padding) {
    int handWidth;
    int handValue;
    int textWidth;
    int textPadding;
    const char *style;
    char statusText[32];

    if (game->player.hands[handIndex].count == 0) {
        return;
    }
    handWidth = game->player.hands[handIndex].count * CARD_WIDTH +
                (game->player.hands[handIndex].count - 1) * CARD_GAP;
    handValue = calculatePlayerHandValueForIndex(game, handIndex);

    if (handValue > 21) {
        snprintf(statusText, sizeof(statusText), "BUST");
    } else if (game->player.hands[handIndex].count == 2 &&
               handValue == 21) {
        snprintf(statusText, sizeof(statusText), "BJ");
    } else {
        snprintf(statusText, sizeof(statusText), "%d", handValue);
    }

    textWidth = (int)strlen(statusText);
    textPadding = (handWidth - textWidth) / 2;
    if (textPadding < 0) {
        textPadding = 0;
    }
    if (handValue > 21) {
        style = scrDanger();
    } else {
        style = handIndex == game->player.activeHand &&
                game->player.activeHand < game->player.handCount
                    ? scrEmphasis() : scrMuted();
    }

    moveToTableRow(PLAYER_CARD_ROW + CARD_HEIGHT, padding);
    printf("%*s%s%s%s", textPadding, "", style, statusText, scrReset());
}

void renderDrawPlayerHands(const Game *game, int padding) {
    int handIndex;
    int handPadding;
    int handWidth;
    int isActive;

    handPadding = padding;
    for (handIndex = 0;
         handIndex < game->player.handCount;
         handIndex++) {
        drawCards(&game->player.hands[handIndex],
                  PLAYER_CARD_ROW, handPadding, 0);
        handWidth = game->player.hands[handIndex].count * CARD_WIDTH +
                    (game->player.hands[handIndex].count - 1) * CARD_GAP;
        handPadding += handWidth + 2;
    }

    handPadding = padding;
    for (handIndex = 0;
         handIndex < game->player.handCount;
         handIndex++) {
        isActive = handIndex == game->player.activeHand &&
                   game->player.activeHand < game->player.handCount;
        moveToTableRow(PLAYER_CARD_ROW - 1, handPadding);
        if (game->player.handCount > 1) {
            if (isActive) {
                printf("%s> HAND %d%s", scrBold(), handIndex + 1, scrReset());
            } else {
                printf("%s  HAND %d%s", scrDim(), handIndex + 1, scrReset());
            }
        }
        printPlayerHandStatus(game, handIndex, handPadding);
        handWidth = game->player.hands[handIndex].count * CARD_WIDTH +
                    (game->player.hands[handIndex].count - 1) * CARD_GAP;
        handPadding += handWidth + 2;
    }
}
