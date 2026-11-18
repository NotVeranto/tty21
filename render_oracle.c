#include "render_oracle.h"

#include "render_control.h"
#include "render_rows.h"
#include "render_style.h"
#include "round_rules.h"

#include <stdio.h>
#include <string.h>

static const char *CARD_RANKS[] = {
    "", "A", "2", "3", "4", "5", "6",
    "7", "8", "9", "10", "J", "Q", "K"
};

static const char *CARD_SUITS[] = {
    "♥", "♦", "♣", "♠"
};

// Moves the cursor to the Oracle information row.
static void moveToOracleRow(int padding) {
    printf(SCR_CURSOR, ROW_ORACLE, padding + 1);
}

// Appends an Oracle item, separated from existing items by " | ".
static void appendOracle(char *buf, size_t size, const char *text) {
    size_t len = strlen(buf);

    if (len == 0) {
        snprintf(buf, size, "%s", text);
    } else if (len + 3 < size) {
        snprintf(buf + len, size - len, " | %s", text);
    }
}

// Reports whether dealt cards are depleted of low ranks.
static int isDeckRunningHot(const Game *game) {
    int lowCount;
    int highCount;
    int i;

    if (game->deck.top < 20) {
        return 0;
    }

    lowCount = 0;
    highCount = 0;

    for (i = 0; i < game->deck.top; i++) {
        Rank rank = game->deck.cards[i].rank;

        if (rank >= TWO && rank <= SIX) {
            lowCount++;
        } else if (rank >= TEN || rank == ACE) {
            highCount++;
        }
    }

    // Hot when lows outnumber highs by more than 1.5x.
    return lowCount * 2 > highCount * 3;
}

// Renders next-card and deck-composition information supplied by Oracle effects.
void renderOracleInfo(const Game *game, int tablePadding) {
    char oracleText[256];

    moveToOracleRow(tablePadding);
    printf(SCR_ERASE_EOL);
    oracleText[0] = '\0';

    if (dealerShowsNextCard(game->run.dealerId) &&
        game->state == STATE_PLAYER_TURN &&
        game->deck.top < DECK_SIZE) {
        snprintf(oracleText,
                 sizeof(oracleText),
                 "Next card: %s%s",
                 CARD_RANKS[game->deck.cards[game->deck.top].rank],
                 CARD_SUITS[game->deck.cards[game->deck.top].suit]);
    }

    if (game->player.perkOwned[PERK_MARKED_CARD] &&
        game->player.perkCharges[PERK_MARKED_CARD] > 0 &&
        game->deck.top < DECK_SIZE) {
        int nextIndex;
        int nextCount;
        int len;

        nextCount = DECK_SIZE - game->deck.top;
        if (nextCount > 3) {
            nextCount = 3;
        }

        len = (int)strlen(oracleText);
        if (len > 0) {
            len += snprintf(oracleText + len,
                            sizeof(oracleText) - (size_t)len,
                            " | Next:");
        } else {
            len += snprintf(oracleText + len,
                            sizeof(oracleText) - (size_t)len,
                            "Next:");
        }

        for (nextIndex = 0; nextIndex < nextCount; nextIndex++) {
            const Card *nextCard;

            nextCard = &game->deck.cards[game->deck.top + nextIndex];
            len += snprintf(oracleText + len,
                            sizeof(oracleText) - (size_t)len,
                            " %s%s",
                            CARD_RANKS[nextCard->rank],
                            CARD_SUITS[nextCard->suit]);
        }
    }

    if (twistActive(game, TWIST_NO_RESHUFFLE) &&
        game->deck.top <= DECK_SIZE) {
        char deckText[32];

        snprintf(deckText, sizeof(deckText),
                 "Deck: %d/%d",
                 DECK_SIZE - game->deck.top,
                 DECK_SIZE);
        appendOracle(oracleText, sizeof(oracleText), deckText);
    }

    if (game->mode == MODE_ROGUELIKE &&
        game->run.floor >= 10 &&
        isDeckRunningHot(game)) {
        appendOracle(oracleText, sizeof(oracleText), "running hot");
    }

    if (oracleText[0] != '\0') {
        printf("%s%s%s", scrDim(), oracleText, scrReset());
    }
}
