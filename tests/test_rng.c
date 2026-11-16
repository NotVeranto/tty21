#include "test.h"
#include "rng.h"
#include "game.h"
#include "card.h"

TEST(rng_range_zero_is_always_zero) {
    int sampleIndex;

    rngInit(101U, 1);

    for (sampleIndex = 0; sampleIndex < 100; sampleIndex++) {
        ASSERT_EQ_INT(0, rngRange(0, 0));
    }
}

TEST(rng_range_single_value_is_inclusive) {
    int sampleIndex;

    rngInit(102U, 1);

    for (sampleIndex = 0; sampleIndex < 100; sampleIndex++) {
        ASSERT_EQ_INT(5, rngRange(5, 5));
    }
}

TEST(rng_range_one_to_ten_bounds_and_diversity) {
    int seen[11] = {0};
    int sampleIndex;
    int value;
    int minimum;
    int maximum;
    int distinct;

    rngInit(103U, 1);
    minimum = 11;
    maximum = 0;
    distinct = 0;

    for (sampleIndex = 0; sampleIndex < 1000; sampleIndex++) {
        value = rngRange(1, 10);
        ASSERT_TRUE(value >= 1 && value <= 10);

        if (value < minimum) {
            minimum = value;
        }
        if (value > maximum) {
            maximum = value;
        }
        if (!seen[value]) {
            seen[value] = 1;
            distinct++;
        }
    }

    ASSERT_TRUE(minimum >= 1);
    ASSERT_TRUE(maximum <= 10);
    ASSERT_TRUE(distinct >= 5);
}

TEST(rng_range_zero_to_ninety_nine_bounds_and_diversity) {
    int seen[100] = {0};
    int sampleIndex;
    int value;
    int minimum;
    int maximum;
    int distinct;

    rngInit(104U, 1);
    minimum = 100;
    maximum = -1;
    distinct = 0;

    for (sampleIndex = 0; sampleIndex < 1000; sampleIndex++) {
        value = rngRange(0, 99);
        ASSERT_TRUE(value >= 0 && value <= 99);

        if (value < minimum) {
            minimum = value;
        }
        if (value > maximum) {
            maximum = value;
        }
        if (!seen[value]) {
            seen[value] = 1;
            distinct++;
        }
    }

    ASSERT_TRUE(minimum >= 0);
    ASSERT_TRUE(maximum <= 99);
    ASSERT_TRUE(distinct >= 5);
}

TEST(rng_same_seed_repeats_first_ten_draws) {
    unsigned int firstSequence[10];
    int sampleIndex;

    rngInit(123456U, 1);
    for (sampleIndex = 0; sampleIndex < 10; sampleIndex++) {
        firstSequence[sampleIndex] = rngNext();
    }

    rngInit(123456U, 1);
    for (sampleIndex = 0; sampleIndex < 10; sampleIndex++) {
        ASSERT_EQ_INT(firstSequence[sampleIndex], rngNext());
    }
}

TEST(rng_seed_repeats_complete_roguelike_initialization) {
    Game firstGame;
    Game secondGame;
    unsigned long firstCallCount;
    int cardIndex;
    int shopIndex;

    rngInit(8675309U, 1);
    initGame(&firstGame, MODE_ROGUELIKE);
    firstCallCount = rngGetCallCount();
    firstGame.rngCalls = firstCallCount;

    rngInit(8675309U, 1);
    initGame(&secondGame, MODE_ROGUELIKE);
    secondGame.rngCalls = rngGetCallCount();

    ASSERT_EQ_INT(firstGame.rngSeed, secondGame.rngSeed);
    ASSERT_EQ_INT(firstCallCount, secondGame.rngCalls);
    ASSERT_EQ_INT(firstGame.deck.top, secondGame.deck.top);
    for (cardIndex = 0; cardIndex < DECK_SIZE; cardIndex++) {
        ASSERT_EQ_INT(firstGame.deck.cards[cardIndex].suit,
                      secondGame.deck.cards[cardIndex].suit);
        ASSERT_EQ_INT(firstGame.deck.cards[cardIndex].rank,
                      secondGame.deck.cards[cardIndex].rank);
        ASSERT_EQ_INT(firstGame.deck.cards[cardIndex].cursed,
                      secondGame.deck.cards[cardIndex].cursed);
    }

    ASSERT_EQ_INT(firstGame.run.shopPerkCount,
                  secondGame.run.shopPerkCount);
    for (shopIndex = 0; shopIndex < firstGame.run.shopPerkCount;
         shopIndex++) {
        ASSERT_EQ_INT(firstGame.run.shopPerks[shopIndex],
                      secondGame.run.shopPerks[shopIndex]);
    }
}

void run_rng_tests(void) {
    RUN_TEST(rng_range_zero_is_always_zero);
    RUN_TEST(rng_range_single_value_is_inclusive);
    RUN_TEST(rng_range_one_to_ten_bounds_and_diversity);
    RUN_TEST(rng_range_zero_to_ninety_nine_bounds_and_diversity);
    RUN_TEST(rng_same_seed_repeats_first_ten_draws);
    RUN_TEST(rng_seed_repeats_complete_roguelike_initialization);
}
