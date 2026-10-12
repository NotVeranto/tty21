#include "test.h"
#include "hand.h"

static Card test_make_card(Rank rank) {
    Card card;

    card.suit = HEARTS;
    card.rank = rank;
    card.cursed = CURSED_NONE;

    return card;
}

TEST(hand_empty_is_zero) {
    Hand hand = {0};

    ASSERT_EQ_INT(0, calculateHandValue(&hand));
}

TEST(hand_simple_sum) {
    Hand hand = {0};
    Card two = test_make_card(TWO);
    Card three = test_make_card(THREE);

    addCardToHand(&hand, &two);
    addCardToHand(&hand, &three);

    ASSERT_EQ_INT(5, calculateHandValue(&hand));
}

TEST(hand_ace_soft_seventeen) {
    Hand hand = {0};
    Card ace = test_make_card(ACE);
    Card six = test_make_card(SIX);

    addCardToHand(&hand, &ace);
    addCardToHand(&hand, &six);

    ASSERT_EQ_INT(17, calculateHandValue(&hand));
}

TEST(hand_ace_hard_on_bust) {
    Hand hand = {0};
    Card ace = test_make_card(ACE);
    Card six = test_make_card(SIX);
    Card ten = test_make_card(TEN);

    addCardToHand(&hand, &ace);
    addCardToHand(&hand, &six);
    addCardToHand(&hand, &ten);

    ASSERT_EQ_INT(17, calculateHandValue(&hand));
}

TEST(hand_two_aces) {
    Hand hand = {0};
    Card ace = test_make_card(ACE);

    addCardToHand(&hand, &ace);
    addCardToHand(&hand, &ace);

    ASSERT_EQ_INT(12, calculateHandValue(&hand));
}

TEST(hand_two_aces_and_nine) {
    Hand hand = {0};
    Card ace = test_make_card(ACE);
    Card nine = test_make_card(NINE);

    addCardToHand(&hand, &ace);
    addCardToHand(&hand, &ace);
    addCardToHand(&hand, &nine);

    ASSERT_EQ_INT(21, calculateHandValue(&hand));
}

TEST(hand_ace_and_king) {
    Hand hand = {0};
    Card ace = test_make_card(ACE);
    Card king = test_make_card(KING);

    addCardToHand(&hand, &ace);
    addCardToHand(&hand, &king);

    ASSERT_EQ_INT(21, calculateHandValue(&hand));
}

TEST(hand_face_cards_bust) {
    Hand hand = {0};
    Card king = test_make_card(KING);
    Card queen = test_make_card(QUEEN);
    Card jack = test_make_card(JACK);

    addCardToHand(&hand, &king);
    addCardToHand(&hand, &queen);
    addCardToHand(&hand, &jack);

    ASSERT_EQ_INT(30, calculateHandValue(&hand));
}

TEST(hand_three_aces_and_eight) {
    Hand hand = {0};
    Card ace = test_make_card(ACE);
    Card eight = test_make_card(EIGHT);

    addCardToHand(&hand, &ace);
    addCardToHand(&hand, &ace);
    addCardToHand(&hand, &ace);
    addCardToHand(&hand, &eight);

    ASSERT_EQ_INT(21, calculateHandValue(&hand));
}

TEST(hand_three_aces_and_nine_downgrades_all_aces) {
    Hand hand = {0};
    Card ace = test_make_card(ACE);
    Card nine = test_make_card(NINE);

    addCardToHand(&hand, &ace);
    addCardToHand(&hand, &ace);
    addCardToHand(&hand, &ace);
    addCardToHand(&hand, &nine);

    /* All three aces can downgrade: 1 + 1 + 1 + 9 = 12. */
    ASSERT_EQ_INT(12, calculateHandValue(&hand));
}

TEST(hand_add_card_updates_count_and_value) {
    Hand hand = {0};
    Card three = test_make_card(THREE);
    Card four = test_make_card(FOUR);

    addCardToHand(&hand, &three);
    addCardToHand(&hand, &four);

    ASSERT_EQ_INT(2, hand.count);
    ASSERT_EQ_INT(7, hand.value);
}

TEST(hand_reset_clears_state) {
    Hand hand = {0};
    Card king = test_make_card(KING);

    addCardToHand(&hand, &king);
    resetHand(&hand);

    ASSERT_EQ_INT(0, hand.count);
    ASSERT_EQ_INT(0, hand.value);
    ASSERT_EQ_INT(0, calculateHandValue(&hand));
}

TEST(hand_add_respects_maximum_size) {
    Hand hand = {0};
    Card two = test_make_card(TWO);
    Card king = test_make_card(KING);
    int cardIndex;

    for (cardIndex = 0; cardIndex < MAX_HAND_SIZE; cardIndex++) {
        addCardToHand(&hand, &two);
    }

    addCardToHand(&hand, &king);

    ASSERT_EQ_INT(MAX_HAND_SIZE, hand.count);
    ASSERT_EQ_INT(MAX_HAND_SIZE * 2, hand.value);
}

void run_hand_tests(void) {
    RUN_TEST(hand_empty_is_zero);
    RUN_TEST(hand_simple_sum);
    RUN_TEST(hand_ace_soft_seventeen);
    RUN_TEST(hand_ace_hard_on_bust);
    RUN_TEST(hand_two_aces);
    RUN_TEST(hand_two_aces_and_nine);
    RUN_TEST(hand_ace_and_king);
    RUN_TEST(hand_face_cards_bust);
    RUN_TEST(hand_three_aces_and_eight);
    RUN_TEST(hand_three_aces_and_nine_downgrades_all_aces);
    RUN_TEST(hand_add_card_updates_count_and_value);
    RUN_TEST(hand_reset_clears_state);
    RUN_TEST(hand_add_respects_maximum_size);
}
