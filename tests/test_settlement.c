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


TEST(settlement_even_money_is_not_paid_twice) {
    Game game;
    settlement_setup(&game);
    settlement_set_player(&game, ACE, KING);
    settlement_set_dealer(&game, ACE, KING);
    game.player.chips = 110;
    game.player.evenMoneyTaken = 1;
    game.roundHadNaturalBj = 1;
    resolveRound(&game);
    ASSERT_EQ_INT(110, game.player.chips);
    ASSERT_EQ_INT(0, game.player.evenMoneyTaken);
    ASSERT_TRUE(strstr(game.lastPayoutBreakdown, "Even money") != NULL);
}

TEST(settlement_surrender_deducts_half_bet_only_once) {
    Game game;
    settlement_setup(&game);
    game.player.chips = 95;
    game.player.surrendered = 1;
    game.roundInProgress = 1;
    resolveRound(&game);
    ASSERT_EQ_INT(95, game.player.chips);
    ASSERT_EQ_INT(0, game.player.surrendered);
    ASSERT_EQ_INT(0, game.roundInProgress);
    ASSERT_EQ_INT(STATE_BET, game.state);
    ASSERT_TRUE(strstr(game.lastPayoutBreakdown, "Surrender") != NULL);
}

TEST(settlement_twin_dealer_resolves_each_half_bet) {
    Game game;
    settlement_setup(&game);
    settlement_set_player(&game, TEN, KING);
    settlement_set_dealer(&game, TEN, EIGHT);
    game.dealer.handCount = 2;
    settlement_add_card(&game.dealer.hands[1], TEN);
    settlement_add_card(&game.dealer.hands[1], NINE);
    resolveRound(&game);
    ASSERT_EQ_INT(110, game.player.chips);
    ASSERT_TRUE(strstr(game.lastPayoutBreakdown, "Won 10") != NULL);
}

TEST(settlement_split_hands_resolve_independently_against_twin) {
    Game game;
    settlement_setup(&game);
    settlement_set_player(&game, TEN, KING);
    game.player.handCount = 2;
    game.player.bets[1] = 10;
    settlement_add_card(&game.player.hands[1], TEN);
    settlement_add_card(&game.player.hands[1], NINE);
    settlement_set_dealer(&game, TEN, EIGHT);
    game.dealer.handCount = 2;
    settlement_add_card(&game.dealer.hands[1], TEN);
    settlement_add_card(&game.dealer.hands[1], TEN);
    resolveRound(&game);
    ASSERT_EQ_INT(105, game.player.chips);
    ASSERT_TRUE(strstr(game.lastPayoutBreakdown, "Won 10, lost 5") != NULL);
}

TEST(settlement_twin_blackjack_insurance_offsets_other_hand_win) {
    Game game;
    settlement_setup(&game);
    settlement_set_player(&game, TEN, KING);
    settlement_set_dealer(&game, ACE, KING);
    game.dealer.handCount = 2;
    settlement_add_card(&game.dealer.hands[1], TEN);
    settlement_add_card(&game.dealer.hands[1], EIGHT);
    game.player.chips = 95;
    game.player.insuranceBet = 5;
    roundSettleInsurance(&game);
    resolveRound(&game);
    ASSERT_EQ_INT(110, game.player.chips);
    ASSERT_EQ_INT(0, game.player.insuranceBet);
}

TEST(settlement_split_double_uses_insurance_perk_once) {
    Game game;
    settlement_setup(&game);
    settlement_set_player(&game, TEN, KING);
    game.player.handCount = 2;
    game.player.bets[0] = 20;
    game.player.usedDouble[0] = 1;
    game.player.bets[1] = 10;
    game.player.usedSplit[0] = 1;
    game.player.usedSplit[1] = 1;
    settlement_add_card(&game.player.hands[1], TEN);
    settlement_add_card(&game.player.hands[1], NINE);
    settlement_set_dealer(&game, ACE, KING);
    game.player.perkOwned[PERK_INSURANCE_PERK] = 1;
    game.player.perkCharges[PERK_INSURANCE_PERK] = 1;
    resolveRound(&game);
    ASSERT_EQ_INT(80, game.player.chips);
    ASSERT_EQ_INT(0, game.player.perkCharges[PERK_INSURANCE_PERK]);
}

TEST(settlement_payout_is_capped_at_twenty_times_the_bet) {
    Game game;
    settlement_setup(&game);
    game.mode = MODE_ROGUELIKE;
    game.run.twistId = TWIST_DESPERATION;
    game.run.extraTwists[0] = TWIST_HIGH_STAKES;
    game.run.extraTwists[1] = TWIST_BLOOD_PACT;
    game.run.extraTwistCount = 2;
    game.run.roundsOnFloor = 1;
    game.player.bets[0] = 20;
    game.player.usedDouble[0] = 1;
    game.winStreak = 3;
    settlement_set_player(&game, TEN, KING);
    settlement_set_dealer(&game, TEN, EIGHT);
    resolveRound(&game);
    ASSERT_EQ_INT(500, game.player.chips);
    ASSERT_TRUE(strstr(game.lastPayoutBreakdown, "Won 400") != NULL);
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
    RUN_TEST(settlement_even_money_is_not_paid_twice);
    RUN_TEST(settlement_surrender_deducts_half_bet_only_once);
    RUN_TEST(settlement_twin_dealer_resolves_each_half_bet);
    RUN_TEST(settlement_split_hands_resolve_independently_against_twin);
    RUN_TEST(settlement_twin_blackjack_insurance_offsets_other_hand_win);
    RUN_TEST(settlement_split_double_uses_insurance_perk_once);
    RUN_TEST(settlement_payout_is_capped_at_twenty_times_the_bet);
}
