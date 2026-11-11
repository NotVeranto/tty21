#include "test.h"
#include "game.h"
#include "hand.h"
#include "round_economy.h"
#include "round_settlement.h"

#include <string.h>

static Card settlement_card(Rank rank) {
    Card card;

    card.suit = HEARTS;
    card.rank = rank;
    card.cursed = CURSED_NONE;

    return card;
}

static void settlement_add_card(Hand *hand, Rank rank) {
    Card card = settlement_card(rank);

    addCardToHand(hand, &card);
}

static void settlement_setup(Game *game) {
    memset(game, 0, sizeof(*game));
    game->mode = MODE_CLASSIC;
    game->state = STATE_RESULT;
    game->run.floor = 1;
    game->run.dealerId = DEALER_CROUPIER;
    game->run.twistId = TWIST_NONE;
    game->minBetMultiplier = 1;
    game->player.chips = 100;
    game->player.handCount = 1;
    game->player.bets[0] = 10;
    game->dealer.handCount = 1;
}

static void settlement_set_player(Game *game,
                                  Rank first,
                                  Rank second) {
    settlement_add_card(&game->player.hands[0], first);
    settlement_add_card(&game->player.hands[0], second);
}

static void settlement_set_dealer(Game *game,
                                  Rank first,
                                  Rank second) {
    settlement_add_card(&game->dealer.hands[0], first);
    settlement_add_card(&game->dealer.hands[0], second);
}

TEST(settlement_natural_blackjack_pays_three_to_two) {
    Game game;

    settlement_setup(&game);
    settlement_set_player(&game, ACE, KING);
    settlement_set_dealer(&game, TEN, EIGHT);
    game.roundHadNaturalBj = 1;

    resolveRound(&game);

    ASSERT_EQ_INT(115, game.player.chips);
}

TEST(settlement_regular_win_pays_one_to_one) {
    Game game;

    settlement_setup(&game);
    settlement_set_player(&game, TEN, KING);
    settlement_set_dealer(&game, TEN, EIGHT);

    resolveRound(&game);

    ASSERT_EQ_INT(110, game.player.chips);
}

TEST(settlement_push_returns_no_net_chips) {
    Game game;

    settlement_setup(&game);
    settlement_set_player(&game, TEN, EIGHT);
    settlement_set_dealer(&game, TEN, EIGHT);

    resolveRound(&game);

    ASSERT_EQ_INT(100, game.player.chips);
    ASSERT_TRUE(strstr(game.lastPayoutBreakdown, "Push") != NULL);
}

TEST(settlement_bust_loses_the_bet) {
    Game game;

    settlement_setup(&game);
    settlement_set_player(&game, TEN, EIGHT);
    settlement_add_card(&game.player.hands[0], FIVE);
    settlement_set_dealer(&game, TEN, EIGHT);

    resolveRound(&game);

    ASSERT_EQ_INT(90, game.player.chips);
}

TEST(settlement_dealer_blackjack_loses_the_bet) {
    Game game;

    settlement_setup(&game);
    settlement_set_player(&game, TEN, EIGHT);
    settlement_set_dealer(&game, ACE, KING);

    resolveRound(&game);

    ASSERT_EQ_INT(90, game.player.chips);
}

TEST(settlement_second_wind_twenty_one_pays_regular_win) {
    Game game;

    settlement_setup(&game);
    settlement_set_player(&game, SEVEN, SEVEN);
    settlement_add_card(&game.player.hands[0], SEVEN);
    settlement_set_dealer(&game, TEN, EIGHT);
    game.player.perkOwned[PERK_SECOND_WIND] = 1;
    game.roundHadNaturalBj = 0;

    resolveRound(&game);

    ASSERT_EQ_INT(110, game.player.chips);
}

TEST(settlement_insurance_pays_three_times_the_stake) {
    Game game;

    settlement_setup(&game);
    settlement_set_player(&game, TEN, EIGHT);
    settlement_set_dealer(&game, ACE, KING);
    game.player.chips = 95;
    game.player.insuranceBet = 5;

    roundSettleInsurance(&game);

    // The 5-chip stake was already deducted; 15 back is a net 2:1 win.
    ASSERT_EQ_INT(110, game.player.chips);
    ASSERT_EQ_INT(0, game.player.insuranceBet);
}

TEST(settlement_insurance_halves_bust_loss_once) {
    Game game;

    settlement_setup(&game);
    settlement_set_player(&game, TEN, EIGHT);
    settlement_add_card(&game.player.hands[0], FIVE);
    settlement_set_dealer(&game, TEN, EIGHT);
    game.player.perkOwned[PERK_INSURANCE_PERK] = 1;
    game.player.perkCharges[PERK_INSURANCE_PERK] = 1;

    resolveRound(&game);

    ASSERT_EQ_INT(95, game.player.chips);
    ASSERT_EQ_INT(0, game.player.perkCharges[PERK_INSURANCE_PERK]);
}

void run_settlement_tests(void) {
    RUN_TEST(settlement_natural_blackjack_pays_three_to_two);
    RUN_TEST(settlement_regular_win_pays_one_to_one);
    RUN_TEST(settlement_push_returns_no_net_chips);
    RUN_TEST(settlement_bust_loses_the_bet);
    RUN_TEST(settlement_insurance_halves_bust_loss_once);
    RUN_TEST(settlement_dealer_blackjack_loses_the_bet);
    RUN_TEST(settlement_second_wind_twenty_one_pays_regular_win);
    RUN_TEST(settlement_insurance_pays_three_times_the_stake);
}
