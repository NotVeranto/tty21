#include "test.h"
#include "card.h"

TEST(deck_initializes_top) {
    Deck deck;

    deck.top = DECK_SIZE;
    initDeck(&deck);

    ASSERT_EQ_INT(0, deck.top);
}

TEST(deck_contains_all_unique_combinations) {
    Deck deck;
    int seen[4][KING + 1] = {{0}};
    int cardIndex;
    int uniqueCount;

    initDeck(&deck);
    uniqueCount = 0;

    for (cardIndex = 0; cardIndex < DECK_SIZE; cardIndex++) {
        Card card = deck.cards[cardIndex];

        ASSERT_TRUE(card.suit >= HEARTS && card.suit <= SPADES);
        ASSERT_TRUE(card.rank >= ACE && card.rank <= KING);
        ASSERT_EQ_INT(0, seen[card.suit][card.rank]);
        seen[card.suit][card.rank] = 1;
        uniqueCount++;
    }

    ASSERT_EQ_INT(52, uniqueCount);
}

TEST(deck_cards_start_uncursed) {
    Deck deck;
    int cardIndex;

    initDeck(&deck);

    for (cardIndex = 0; cardIndex < DECK_SIZE; cardIndex++) {
        ASSERT_EQ_INT(CURSED_NONE, deck.cards[cardIndex].cursed);
    }
}

TEST(card_ace_value_is_eleven) {
    Card card = {HEARTS, ACE, CURSED_NONE};

    ASSERT_EQ_INT(11, getCardValue(&card));
}

TEST(card_numeric_values_match_rank) {
    Rank rank;

    for (rank = TWO; rank <= TEN; rank++) {
        Card card = {HEARTS, rank, CURSED_NONE};

        ASSERT_EQ_INT((int)rank, getCardValue(&card));
    }
}

TEST(card_face_values_are_ten) {
    Card jack = {HEARTS, JACK, CURSED_NONE};
    Card queen = {HEARTS, QUEEN, CURSED_NONE};
    Card king = {HEARTS, KING, CURSED_NONE};

    ASSERT_EQ_INT(10, getCardValue(&jack));
    ASSERT_EQ_INT(10, getCardValue(&queen));
    ASSERT_EQ_INT(10, getCardValue(&king));
}

void run_card_tests(void) {
    RUN_TEST(deck_initializes_top);
    RUN_TEST(deck_contains_all_unique_combinations);
    RUN_TEST(deck_cards_start_uncursed);
    RUN_TEST(card_ace_value_is_eleven);
    RUN_TEST(card_numeric_values_match_rank);
    RUN_TEST(card_face_values_are_ten);
}
